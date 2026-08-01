// Breachpoint. Definition of the native UI layer tags.

#include "UI/BRUITypes.h"

/**
 * The one instance. Its base constructor (FGameplayTagNativeAdder, GameplayTagContainer.h)
 * registers AddTags() with UGameplayTagsManager's last-chance-to-add-native-tags delegate, so
 * the four UI.Layer.* tags exist before any widget or ini asks for them, with no
 * DefaultGameplayTags.ini entry and no load-order gamble.
 *
 * This is the same shape CommonUI uses for its own FGlobalUITags (UITag.h) - deliberately, so
 * the pattern is one someone can look up in the engine rather than one we invented.
 */
FBRUITags FBRUITags::Singleton;
