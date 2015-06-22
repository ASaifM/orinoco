/* This file should serve as a replacement to cfg.c */


#include <linux/moduleparam.h>
#include <linux/inetdevice.h>
#include <linux/export.h>

#include "core.h"
#include "cfg80211.h"
//#include "debug.h"
//#include "hif-ops.h"
//#include "testmode.h"


/* hif layer decides what suspend mode to use */
/* not there yet */ 
static int __orinoco_cfg80211_suspend(struct wiphy *wiphy,
				 struct cfg80211_wowlan *wow)
{
	struct orinoco_private *priv = wiphy_priv(wiphy);

	return 0;
}


int orinoco_cfg80211_resume(struct orinoco_private *priv)
{
	return 0;
}

static int __orinoco_cfg80211_resume(struct wiphy *wiphy)
{
	struct orinoco_private *priv = wiphy_priv(wiphy);
	int err;

	return 0;
}

static bool orinoco_is_p2p_ie(const u8 *pos)
{
	return 0;
}

static int orinoco_get_rsn_capab(struct cfg80211_beacon_data *beacon,
				u8 *rsn_capab)
{
	const u8 *rsn_ie;
	size_t rsn_ie_len;
	u16 cnt;

	
	return 0;
}

static int orinoco_start_ap(struct wiphy *wiphy, struct net_device *dev,
			   struct cfg80211_ap_settings *info)
{
	struct orinoco_private *priv = wiphy_priv(wiphy);
	return 0;
}

static int orinoco_change_beacon(struct wiphy *wiphy, struct net_device *dev,
				struct cfg80211_beacon_data *beacon)
{
	
}

/* Will this actually work for orinoco? */
static int orinoco_stop_ap(struct wiphy *wiphy, struct net_device *dev)
{
	struct orinoco_private *priv = ath6kl_priv(dev);
	return 0;
}

static const u8 bcast_addr[ETH_ALEN] = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };

static int orinoco_del_station(struct wiphy *wiphy, struct net_device *dev,
			      struct station_del_parameters *params)
{
	return 0;
}

static int orinoco_change_station(struct wiphy *wiphy, struct net_device *dev,
				 const u8 *mac,
				 struct station_parameters *params)
{
	struct orinoco_priv *priv = orinoco_priv(dev);
	
}

static int orinoco_remain_on_channel(struct wiphy *wiphy,
				    struct wireless_dev *wdev,
				    struct ieee80211_channel *chan,
				    unsigned int duration,
				    u64 *cookie)
{
	return 0;
}

static int orinoco_cancel_remain_on_channel(struct wiphy *wiphy,
					   struct wireless_dev *wdev,
					   u64 cookie)
{
	return 0;
}



/* Check if SSID length is greater than DIRECT- */
static bool orinoco_is_p2p_go_ssid(const u8 *buf, size_t len)
{
	return false;
}

static int orinoco_mgmt_tx(struct wiphy *wiphy, struct wireless_dev *wdev,
			  struct cfg80211_mgmt_tx_params *params, u64 *cookie)
{
	return 0;
}

static void orinoco_mgmt_frame_register(struct wiphy *wiphy,
				       struct wireless_dev *wdev,
				       u16 frame_type, bool reg)
{

}

static int orinoco_cfg80211_sscan_start(struct wiphy *wiphy,
			struct net_device *dev,
			struct cfg80211_sched_scan_request *request)
{
	struct orinoco_private *priv = orinoco_priv(dev);


	return 0;
}

static int orinoco_cfg80211_sscan_stop(struct wiphy *wiphy,
				      struct net_device *dev)
{
	return 0;
}

static int orinoco_cfg80211_set_bitrate(struct wiphy *wiphy,
				       struct net_device *dev,
				       const u8 *addr,
				       const struct cfg80211_bitrate_mask *mask)
{
	return 0;
}

static int orinoco_cfg80211_set_txe_config(struct wiphy *wiphy,
					  struct net_device *dev,
					  u32 rate, u32 pkts, u32 intvl)
{
	return 0;
}

/* As every function is implemented, it will be 
added to the ops struct
*/
static struct cfg80211_ops orinoco_cfg80211_ops = {
//	.suspend = ,
//	.resume = ,
// 	set_wakeup =,

	
};

void orinoco_cfg80211_stop_all(struct orinoco_private *priv)
{

}

static void orinoco_cfg80211_reg_notify(struct wiphy *wiphy,
				       struct regulatory_request *request)
{
	struct orinoco_private *priv = wiphy_priv(wiphy);
}




struct wireless_dev *orinoco_interface_add(struct orinoco_private *priv, const char *name,
					  unsigned char name_assign_type,
					  enum nl80211_iftype type,
					  u8 fw_vif_idx, u8 nw_type)
{
	struct net_device *ndev;
	return NULL;
}

int orinoco_cfg80211_init(struct orinoco_private *priv)
{
	struct wiphy *wiphy;
	int ret;

	/* set device pointer for wiphy */
	set_wiphy_dev(wiphy, priv->dev);
	ret = wiphy_register(wiphy);
	if (ret < 0) {
		pr_err("couldn't register wiphy device\n");
		return ret;
	}


	return 0;
}

void orinoco_cfg80211_cleanup(struct orinoco_private *priv)
{
	struct wiphy *wiphy = priv_to_wiphy(priv);
	wiphy_unregister(wiphy);
}

struct orinoco_private *orinoco_cfg80211_create(void)
{
	struct orinoco_private *priv;
	struct wiphy *wiphy;

	/* create a new wiphy for use with cfg80211 */
	wiphy = wiphy_new(&orinoco_cfg80211_ops, sizeof(struct orinoco_private));

	if (!wiphy) {
		pr_err("couldn't allocate wiphy device\n");
		return NULL;
	}

	priv = wiphy_priv(wiphy);
	priv->wiphy = wiphy;

	return priv;
}

void orinoco_cfg80211_destroy(struct orinoco_private *priv)
{
	struct wiphy *wiphy = priv_to_wiphy(priv);

	wiphy_unregister(wiphy);
	wiphy_free(wiphy);
}

