/* cfg80211 support.
 *
 * See copyright notice in main.c
 */
#ifndef ORINOCO_CFG_H
#define ORINOCO_CFG_H

#include <net/cfg80211.h>

extern const struct cfg80211_ops orinoco_cfg_ops;

void orinoco_wiphy_init(struct wiphy *wiphy);
int orinoco_wiphy_register(struct wiphy *wiphy);

#endif /* ORINOCO_CFG_H */



/* This file should serve as a replacement to cfg.h */

struct wireless_dev *orinoco_interface_add(struct orinoco_private *priv, const char *name,
					  unsigned char name_assign_type,
					  enum nl80211_iftype type,
					  u8 fw_vif_idx, u8 nw_type);
/*void ath6kl_cfg80211_ch_switch_notify(struct ath6kl_vif *vif, int freq,
				      enum wmi_phy_mode mode);
void ath6kl_cfg80211_scan_complete_event(struct ath6kl_vif *vif, bool aborted);

void ath6kl_cfg80211_connect_event(struct ath6kl_vif *vif, u16 channel,
				   u8 *bssid, u16 listen_intvl,
				   u16 beacon_intvl,
				   enum network_type nw_type,
				   u8 beacon_ie_len, u8 assoc_req_len,
				   u8 assoc_resp_len, u8 *assoc_info);

void ath6kl_cfg80211_disconnect_event(struct ath6kl_vif *vif, u8 reason,
				      u8 *bssid, u8 assoc_resp_len,
				      u8 *assoc_info, u16 proto_reason);

void ath6kl_cfg80211_tkip_micerr_event(struct ath6kl_vif *vif, u8 keyid,
				     bool ismcast);

int ath6kl_cfg80211_suspend(struct ath6kl *ar,
			    enum ath6kl_cfg_suspend_mode mode,
			    struct cfg80211_wowlan *wow);
*/
int orinoco_cfg80211_resume(struct orinoco_private *priv);

/*
void ath6kl_cfg80211_vif_cleanup(struct ath6kl_vif *vif);

void ath6kl_cfg80211_stop(struct ath6kl_vif *vif);*/
void orinoco_cfg80211_stop_all(struct orinoco_private *priv);

int orinoco_cfg80211_init(struct orinoco_private *priv);
void orinoco_cfg80211_cleanup(struct orinoco_private *priv);

struct orinoco_private *orinoco_cfg80211_create(void);
void orinoco_cfg80211_destroy(struct orinoco_private *orinoco_private);


