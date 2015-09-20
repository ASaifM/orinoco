/* cfg80211 support
 *
 * See copyright notice in main.c
 */
#include <linux/etherdevice.h>
#include <linux/ieee80211.h>
#include <net/cfg80211.h>
#include "hw.h"
#include "main.h"
#include "orinoco.h"

#include "cfg.h"

/* Supported bitrates. Must agree with hw.c */
static struct ieee80211_rate orinoco_rates[] = {
	{ .bitrate = 10 },
	{ .bitrate = 20 },
	{ .bitrate = 55 },
	{ .bitrate = 110 },
};

static const void * const orinoco_wiphy_privid = &orinoco_wiphy_privid;

/* Called after orinoco_private is allocated. */
void orinoco_wiphy_init(struct wiphy *wiphy)
{
	struct orinoco_private *priv = wiphy_priv(wiphy);

	wiphy->privid = orinoco_wiphy_privid;

	set_wiphy_dev(wiphy, priv->dev);
}

/* Called after firmware is initialised */
int orinoco_wiphy_register(struct wiphy *wiphy)
{
	struct orinoco_private *priv = wiphy_priv(wiphy);
	int i, channels = 0;

	if (priv->firmware_type == FIRMWARE_TYPE_AGERE)
		wiphy->max_scan_ssids = 1;
	else
		wiphy->max_scan_ssids = 0;

	wiphy->interface_modes = BIT(NL80211_IFTYPE_STATION);

	/* TODO: should we set if we only have demo ad-hoc?
	 *       (priv->has_port3)
	 */
	if (priv->has_ibss)
		wiphy->interface_modes |= BIT(NL80211_IFTYPE_ADHOC);

	if (!priv->broken_monitor || force_monitor)
		wiphy->interface_modes |= BIT(NL80211_IFTYPE_MONITOR);

	priv->band.bitrates = orinoco_rates;
	priv->band.n_bitrates = ARRAY_SIZE(orinoco_rates);

	/* Only support channels allowed by the card EEPROM */
	for (i = 0; i < NUM_CHANNELS; i++) {
		if (priv->channel_mask & (1 << i)) {
			priv->channels[i].center_freq =
				ieee80211_channel_to_frequency(i + 1,
							   IEEE80211_BAND_2GHZ);
			channels++;
		}
	}
	priv->band.channels = priv->channels;
	priv->band.n_channels = channels;

	wiphy->bands[IEEE80211_BAND_2GHZ] = &priv->band;
	wiphy->signal_type = CFG80211_SIGNAL_TYPE_MBM;

	i = 0;
	if (priv->has_wep) {
		priv->cipher_suites[i] = WLAN_CIPHER_SUITE_WEP40;
		i++;

		if (priv->has_big_wep) {
			priv->cipher_suites[i] = WLAN_CIPHER_SUITE_WEP104;
			i++;
		}
	}
	if (priv->has_wpa) {
		priv->cipher_suites[i] = WLAN_CIPHER_SUITE_TKIP;
		i++;
	}
	wiphy->cipher_suites = priv->cipher_suites;
	wiphy->n_cipher_suites = i;

	wiphy->rts_threshold = priv->rts_thresh;
	if (!priv->has_mwo)
		wiphy->frag_threshold = priv->frag_thresh + 1;
	wiphy->retry_short = priv->short_retry_limit;
	wiphy->retry_long = priv->long_retry_limit;

	return wiphy_register(wiphy);
}

static int orinoco_change_vif(struct wiphy *wiphy, struct net_device *dev,
			      enum nl80211_iftype type, u32 *flags,
			      struct vif_params *params)
{
	struct orinoco_private *priv = wiphy_priv(wiphy);
	int err = 0;
	unsigned long lock;

	if (orinoco_lock(priv, &lock) != 0)
		return -EBUSY;

	switch (type) {
	case NL80211_IFTYPE_ADHOC:
		if (!priv->has_ibss && !priv->has_port3)
			err = -EINVAL;
		break;

	case NL80211_IFTYPE_STATION:
		break;

	case NL80211_IFTYPE_MONITOR:
		if (priv->broken_monitor && !force_monitor) {
			wiphy_warn(wiphy,
				   "Monitor mode support is buggy in this firmware, not enabling\n");
			err = -EINVAL;
		}
		break;

	default:
		err = -EINVAL;
	}

	if (!err) {
		priv->iw_mode = type;
		set_port_type(priv);
		err = orinoco_commit(priv);
	}

	orinoco_unlock(priv, &lock);

	return err;
}

static int orinoco_scan(struct wiphy *wiphy,
			struct cfg80211_scan_request *request)
{
	struct orinoco_private *priv = wiphy_priv(wiphy);
	int err;

	if (!request)
		return -EINVAL;

	if (priv->scan_request && priv->scan_request != request)
		return -EBUSY;

	priv->scan_request = request;

	err = orinoco_hw_trigger_scan(priv, request->ssids);
	/* On error the we aren't processing the request */
	if (err)
		priv->scan_request = NULL;

	return err;
}

static int orinoco_set_monitor_channel(struct wiphy *wiphy,
				       struct cfg80211_chan_def *chandef)
{
	struct orinoco_private *priv = wiphy_priv(wiphy);
	int err = 0;
	unsigned long flags;
	int channel;

	if (!chandef->chan)
		return -EINVAL;

	if (cfg80211_get_chandef_type(chandef) != NL80211_CHAN_NO_HT)
		return -EINVAL;

	if (chandef->chan->band != IEEE80211_BAND_2GHZ)
		return -EINVAL;

	channel = ieee80211_frequency_to_channel(chandef->chan->center_freq);

	if ((channel < 1) || (channel > NUM_CHANNELS) ||
	     !(priv->channel_mask & (1 << (channel - 1))))
		return -EINVAL;

	if (orinoco_lock(priv, &flags) != 0)
		return -EBUSY;

	priv->channel = channel;
	if (priv->iw_mode == NL80211_IFTYPE_MONITOR) {
		/* Fast channel change - no commit if successful */
		struct hermes *hw = &priv->hw;
		err = hw->ops->cmd_wait(hw, HERMES_CMD_TEST |
					    HERMES_TEST_SET_CHANNEL,
					channel, NULL);
	}
	orinoco_unlock(priv, &flags);

	return err;
}

static int orinoco_set_wiphy_params(struct wiphy *wiphy, u32 changed)
{
	struct orinoco_private *priv = wiphy_priv(wiphy);
	int frag_value = -1;
	int rts_value = -1;
	int err = 0;

	if (changed & WIPHY_PARAM_RETRY_SHORT) {
		/* Setting short retry not supported */
		err = -EINVAL;
	}

	if (changed & WIPHY_PARAM_RETRY_LONG) {
		/* Setting long retry not supported */
		err = -EINVAL;
	}

	if (changed & WIPHY_PARAM_FRAG_THRESHOLD) {
		/* Set fragmentation */
		if (priv->has_mwo) {
			if (wiphy->frag_threshold < 0)
				frag_value = 0;
			else {
				printk(KERN_WARNING "%s: Fixed fragmentation "
				       "is not supported on this firmware. "
				       "Using MWO robust instead.\n",
				       priv->ndev->name);
				frag_value = 1;
			}
		} else {
			if (wiphy->frag_threshold < 0)
				frag_value = 2346;
			else if ((wiphy->frag_threshold < 257) ||
				 (wiphy->frag_threshold > 2347))
				err = -EINVAL;
			else
				/* cfg80211 value is 257-2347 (odd only)
				 * orinoco rid has range 256-2346 (even only) */
				frag_value = wiphy->frag_threshold & ~0x1;
		}
	}

	if (changed & WIPHY_PARAM_RTS_THRESHOLD) {
		/* Set RTS.
		 *
		 * Prism documentation suggests default of 2432,
		 * and a range of 0-3000.
		 *
		 * Current implementation uses 2347 as the default and
		 * the upper limit.
		 */

		if (wiphy->rts_threshold < 0)
			rts_value = 2347;
		else if (wiphy->rts_threshold > 2347)
			err = -EINVAL;
		else
			rts_value = wiphy->rts_threshold;
	}

	if (!err) {
		unsigned long flags;

		if (orinoco_lock(priv, &flags) != 0)
			return -EBUSY;

		if (frag_value >= 0) {
			if (priv->has_mwo)
				priv->mwo_robust = frag_value;
			else
				priv->frag_thresh = frag_value;
		}
		if (rts_value >= 0)
			priv->rts_thresh = rts_value;

		err = orinoco_commit(priv);

		orinoco_unlock(priv, &flags);
	}

	return err;
}


static int orinoco_set_wpa_version(struct orinoco_private *priv, enum nl80211_wpa_versions wpa_version){

	if (!wpa_version) {
		priv->encode_alg = ORINOCO_ALG_NONE;
	} else if (wpa_version & NL80211_WPA_VERSION_1) {
		priv->encode_alg = ORINOCO_ALG_TKIP;
	} else {
		netdev_err(priv->ndev, "Orinoco supports TKIP only. \n");
		return -ENOTSUPP;
	}

	return 0;
}


static int orinoco_set_authentication(struct orinoco_private *priv, enum nl80211_auth_type auth_type){

	switch (auth_type){
	case NL80211_AUTHTYPE_OPEN_SYSTEM:
		priv->wep_restrict = 0;
		break;
	case NL80211_AUTHTYPE_SHARED_KEY:
		priv->wep_restrict = 1;
		break;
	case NL80211_AUTHTYPE_AUTOMATIC:
		/* set to Shared key but that needs to change. */
		/* can be open or shared so? */
		/* change the way wep_restrict is handled */
		priv->wep_restrict = 1;
		break;

	default:
		printk(KERN_ERR "Authentication method %d is not supported\n", auth_type);
		return -ENOTSUPP;
	}

	return 0;
}

static int orinoco_cfg80211_connect(struct wiphy *wiphy, struct net_device *dev,
				   struct cfg80211_connect_params *sme)
{
	struct orinoco_private *priv = wiphy_priv(wiphy);
	unsigned long flags = 0;
	int ret = 0;
	/*test if card is trying to connect or already connected. If so
	 *return error code. orinoco_private supports that?
	 * Need to check and modify struct accordingly.
         */
	printk(KERN_WARNING "In orinoco cfg80211 connect \n");
	if (orinoco_lock(priv, &flags) != 0)
		return -EBUSY;

	if (priv->sme_state == SME_CONNECTING || priv->sme_state == SME_CONNECTED){
		netdev_err(dev, "Card already established a connection or trying to! \n");
		return -EBUSY;
	}
	/*New note: orinoco_private needs to change drastically! */

	/* If we don't have a valid ssid, we shouldn't connect! */
	if (!sme->ssid){
		netdev_err(dev, "Invalid ssid\n");
		return -EPERM;
	}

	if (sme->bssid && is_zero_ether_addr(sme->bssid)){
		netdev_err(dev, "Zero address bssid! \n");
		return -EINVAL;
	}

	/* need to check if the card is already connected
	 * and reconnect
	 */
	/* code for reconnection to be added */
	priv->sme_state = SME_CONNECTING;

	if (sme->channel){
		int channel;

		channel = ieee80211_frequency_to_channel(sme->channel->center_freq);

		if ((channel > 0) && (channel < NUM_CHANNELS) &&
		    (priv->channel_mask & (1 << channel)))
			priv->channel = channel;
		priv->ch_hint = sme->channel->center_freq;
	}

	memset(priv->ssid, 0, sizeof(priv->ssid));
	priv->ssid_len = sme->ssid_len;
	memcpy(priv->ssid, sme->ssid, sme->ssid_len);

	eth_zero_addr(priv->desired_bssid);
	priv->bssid_fixed = 0;
	if (sme->bssid && !is_broadcast_ether_addr(sme->bssid)){
		ether_addr_copy(priv->desired_bssid, sme->bssid);
		priv->bssid_fixed = 1;
	}

	/* set authentication */
	ret = orinoco_set_wpa_version(priv, sme->crypto.wpa_versions);
	if (ret)
		goto out;
	/* need to set the authenticatin type
	 * What are the supported authentication
	 * methods by Orinoco?
	 */
	ret = orinoco_set_authentication(priv, sme->auth_type);
	if (ret)
		goto out;
	orinoco_unlock(priv, &flags);
	schedule_work(&priv->join_work);
	/*if (orinoco_lock(priv, &flags) != 0)
		return -EBUSY; */
	ret = orinoco_commit(priv);
	if (ret) {
		printk(KERN_ERR "We failed to connect \n");
		cfg80211_connect_result(dev, priv->desired_bssid, NULL, 0, NULL, 0, WLAN_STATUS_UNSPECIFIED_FAILURE, GFP_KERNEL);
	}
	else {
		printk(KERN_ERR "Connection seems to have succeeded \n");
		cfg80211_connect_result(dev, priv->desired_bssid, NULL, 0, NULL, 0, WLAN_STATUS_SUCCESS, GFP_KERNEL);
	}
	/* orinoco_unlock(priv, &flags); */
	return ret;

out:
	orinoco_unlock(priv, &flags);
	return ret;
}

static int orinoco_cfg80211_disconnect(struct wiphy *wiphy,
				      struct net_device *dev, u16 reason_code)
{
	struct orinoco_private *priv = wiphy_priv(wiphy);
	int ret = 0;

	memset(priv->ssid, 0, sizeof(priv->ssid));
	priv->ssid_len = 0;

	eth_zero_addr(priv->desired_bssid);
	priv->sme_state = SME_DISCONNECTED;

	return ret;
}

static int orinoco_cfg80211_join_ibss(struct wiphy *wiphy,
				     struct net_device *dev,
				     struct cfg80211_ibss_params *ibss_param)
{

	struct orinoco_private *priv = wiphy_priv(wiphy);
	unsigned long flags = 0;
	int ret = 0;

	if (orinoco_lock(priv, &flags) != 0)
		return -EBUSY;

	if (!priv->has_ibss){
		netdev_err(dev, "ibss not supported! \n");
		return -ENOTSUPP;
	}

	memset(priv->ssid, 0, sizeof(priv->ssid));
	priv->ssid_len = ibss_param->ssid_len;
	memcpy(priv->ssid, ibss_param->ssid, ibss_param->ssid_len);

	eth_zero_addr(priv->desired_bssid);
	priv->bssid_fixed = 0;
	if (ibss_param->bssid && !is_broadcast_ether_addr(ibss_param->bssid)){
		ether_addr_copy(priv->desired_bssid, ibss_param->bssid);
		priv->bssid_fixed = 1;
	}

	ret = orinoco_commit(priv);
	orinoco_unlock(priv, &flags);

	return ret;
}

static int orinoco_cfg80211_leave_ibss(struct wiphy *wiphy,
				      struct net_device *dev)
{
	struct orinoco_private *priv = wiphy_priv(wiphy);
	int ret = 0;

	memset(priv->ssid, 0, sizeof(priv->ssid));
	priv->ssid_len = 0;
	eth_zero_addr(priv->desired_bssid);



	return ret;
}

const struct cfg80211_ops orinoco_cfg_ops = {
	.change_virtual_intf = orinoco_change_vif,
	.connect = orinoco_cfg80211_connect,
	.disconnect = orinoco_cfg80211_disconnect,
	.join_ibss = orinoco_cfg80211_join_ibss,
	.leave_ibss = orinoco_cfg80211_leave_ibss,
	.set_monitor_channel = orinoco_set_monitor_channel,
	.scan = orinoco_scan,
	.set_wiphy_params = orinoco_set_wiphy_params,
};
