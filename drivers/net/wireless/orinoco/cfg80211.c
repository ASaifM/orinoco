/* cfg80211 support
 * Currently this file replaces cfg.c
 * See copyright notice in main.c
 */
#include <linux/ieee80211.h>
#include <net/cfg80211.h>
#include "hw.h"
#include "main.h"
#include "orinoco.h"
#include <linux/moduleparam.h>
#include <linux/inetdevice.h>
#include <linux/export.h>

//#include "core.h"
#include "cfg80211.h"
#include "fw.h"
#include "mic.h"
//#include "debug.h"
//#include "hif-ops.h"
//#include "testmode.h"

/* Supported bitrates. Must agree with hw.c */
static struct ieee80211_rate orinoco_rates[] = {
	{ .bitrate = 10 },
	{ .bitrate = 20 },
	{ .bitrate = 55 },
	{ .bitrate = 110 },
};

static const void * const orinoco_wiphy_privid = &orinoco_wiphy_privid;

/* Called after orinoco_private is allocated. */
/* This function simply provides the wiphy with a unique id and 
associates it with the device used by the driver's private struct */
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

	if (priv->firmware_type == FIRMWARE_TYPE_AGERE){
		wiphy->max_scan_ssids = 1;
		printk(KERN_DEBUG "we have an agere here \n");
	}
	else
		wiphy->max_scan_ssids = 0;

	printk(KERN_DEBUG "firmware is %d \n", priv->firmware_type);

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


static const u8 bcast_addr[ETH_ALEN] = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };

static int orinoco_scan(struct wiphy *wiphy,
			struct cfg80211_scan_request *request)
{
	struct orinoco_private *priv = wiphy_priv(wiphy);
	int err;

	if (!request)
		return -EINVAL;

	if (request->n_channels > 0)
		printk (KERN_DEBUG "No. of channels is %d", request->n_channels);

	if (priv->scan_request && priv->scan_request != request)
		return -EBUSY;

	priv->scan_request = request;

	err = orinoco_hw_trigger_scan(priv, request->ssids);
	/* On error the we aren't processing the request */
	if (err)
		priv->scan_request = NULL;

	return err;
}




/* As every function is implemented, it will be 
added to the ops struct
*/
static struct cfg80211_ops orinoco_cfg80211_ops = {
/*	.suspend = ,
	.resume = ,
 	set_wakeup =,
	.join_ibss = orinoco_cfg80211_join_ibss,*/
	.scan = orinoco_scan,
		
};

int orinoco_cfg80211_init(struct orinoco_private *priv)
{
	struct device *dev = priv->dev;
	struct wiphy *wiphy = priv_to_wiphy(priv);
	struct hermes *hw = &priv->hw;
	int err = 0;

	/* No need to lock, the hw_unavailable flag is already set in
	 * alloc_orinocodev() */
	priv->nicbuf_size = IEEE80211_MAX_FRAME_LEN + ETH_HLEN;

	/* Initialize the firmware */
	err = hw->ops->init(hw);
	if (err != 0) {
		dev_err(dev, "Failed to initialize firmware (err = %d)\n",
			err);
		goto out;
	}

	err = determine_fw_capabilities(priv, wiphy->fw_version,
					sizeof(wiphy->fw_version),
					&wiphy->hw_version);
	if (err != 0) {
		dev_err(dev, "Incompatible firmware, aborting\n");
		goto out;
	}

	if (priv->do_fw_download) {
#ifdef CONFIG_HERMES_CACHE_FW_ON_INIT
		orinoco_cache_fw(priv, 0);
#endif

		err = orinoco_download(priv);
		if (err)
			priv->do_fw_download = 0;

		/* Check firmware version again */
		err = determine_fw_capabilities(priv, wiphy->fw_version,
						sizeof(wiphy->fw_version),
						&wiphy->hw_version);
		if (err != 0) {
			dev_err(dev, "Incompatible firmware, aborting\n");
			goto out;
		}
	}

	if (priv->has_port3)
		dev_info(dev, "Ad-hoc demo mode supported\n");
	if (priv->has_ibss)
		dev_info(dev, "IEEE standard IBSS ad-hoc mode supported\n");
	if (priv->has_wep)
		dev_info(dev, "WEP supported, %s-bit key\n",
			 priv->has_big_wep ? "104" : "40");
	if (priv->has_wpa) {
		dev_info(dev, "WPA-PSK supported\n");
		if (orinoco_mic_init(priv)) {
			dev_err(dev, "Failed to setup MIC crypto algorithm. "
				"Disabling WPA support\n");
			priv->has_wpa = 0;
		}
	}

	err = orinoco_hw_read_card_settings(priv, wiphy->perm_addr);
	if (err)
		goto out;

	err = orinoco_hw_allocate_fid(priv);
	if (err) {
		dev_err(dev, "Failed to allocate NIC buffer!\n");
		goto out;
	}

	/* Set up the default configuration */
	priv->iw_mode = NL80211_IFTYPE_STATION;
	/* By default use IEEE/IBSS ad-hoc mode if we have it */
	priv->prefer_port3 = priv->has_port3 && (!priv->has_ibss);
	set_port_type(priv);
	priv->channel = 0; /* use firmware default */

	priv->promiscuous = 0;
	priv->encode_alg = ORINOCO_ALG_NONE;
	priv->tx_key = 0;
	priv->wpa_enabled = 0;
	priv->tkip_cm_active = 0;
	priv->key_mgmt = 0;
	priv->wpa_ie_len = 0;
	priv->wpa_ie = NULL;

	if (orinoco_wiphy_register(wiphy)) {
		err = -ENODEV;
		goto out;
	}

	/* Make the hardware available, as long as it hasn't been
	 * removed elsewhere (e.g. by PCMCIA hot unplug) */
	orinoco_lock_irq(priv);
	priv->hw_unavailable--;
	orinoco_unlock_irq(priv);

	dev_dbg(dev, "Ready\n");

 out:
	return err;
}
EXPORT_SYMBOL(orinoco_cfg80211_init);

void orinoco_cfg80211_cleanup(struct orinoco_private *priv)
{
	struct wiphy *wiphy = priv_to_wiphy(priv);

	wiphy_unregister(wiphy);
}

/* will borrow code from alloc_orinocodev */
struct orinoco_private *orinoco_cfg80211_create(void)
{
	struct orinoco_private *priv;
	struct wiphy *wiphy;
	int err;

	/* create a new wiphy for use with cfg80211 */
	wiphy = wiphy_new(&orinoco_cfg80211_ops, sizeof(struct orinoco_private));

	if (!wiphy) {
		pr_err("couldn't allocate wiphy device\n");
		return NULL;
	}

	priv = wiphy_priv(wiphy);
	err = orinoco_cfg80211_init(priv);

	return priv;
}

void orinoco_cfg80211_destroy(struct orinoco_private *priv)
{
	struct wiphy *wiphy = priv_to_wiphy(priv);

	wiphy_unregister(wiphy);
	wiphy_free(wiphy);
}
