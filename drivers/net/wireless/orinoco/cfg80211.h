/* cfg80211 support.
 *
 * See copyright notice in main.c
 */
#ifndef ORINOCO_CFG80211_H
#define ORINOCO_CFG80211_H

#include <net/cfg80211.h>

static struct cfg80211_ops orinoco_cfg80211_ops;

void orinoco_wiphy_init(struct wiphy *wiphy);
int orinoco_wiphy_register(struct wiphy *wiphy);

int orinoco_cfg80211_init(struct orinoco_private *priv);
void orinoco_cfg80211_cleanup(struct orinoco_private *priv);

struct orinoco_private *orinoco_cfg80211_create(void);
void orinoco_cfg80211_destroy(struct orinoco_private *orinoco_private);

#endif /* ATH6KL_CFG80211_H */
