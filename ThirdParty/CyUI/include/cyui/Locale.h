#pragma once
#include "cyui/Export.h"
#include <string>
#include <unordered_map>
#include <vector>

namespace cyui {
struct LocaleInfo {
    std::string code, name;
};
using TranslationCatalog = std::unordered_map<std::string, std::string>;
/**
 * @doc api-set-locale
 * @locale en
 * @title setLocale
 * @section API
 * @symbol cyui::setLocale
 * Select the language of the current ImGui context; English is the default.
 *
 * ## Contract {#contract}
 * `bool setLocale(const char* code)` returns false for an unknown language and
 * preserves the current selection. Regional tags fall back to registered base
 * languages. Without a current context, update defaults and live contexts only
 * outside concurrent rendering. French is built in. Catalogs are shared.
 * Use `tr` for display text and `trLabel` to retain interactive widget identities.
 * [Complete localization guide](doc:localization).
 */
/**
 * @doc api-set-locale
 * @locale fr
 * @title setLocale
 * @section API
 * @symbol cyui::setLocale
 * Choisit la langue du contexte ImGui courant ; l’anglais est le défaut.
 *
 * ## Contrat {#contract}
 * `bool setLocale(const char* code)` retourne false pour une langue inconnue et
 * conserve la sélection. Les variantes régionales se replient sur leur langue
 * enregistrée. Sans contexte courant, changez valeurs par défaut et contextes
 * existants hors rendu concurrent. Le français est intégré ; les catalogues sont
 * partagés. `tr` traduit l’affichage et `trLabel` préserve l’identité des contrôles.
 * [Guide complet](doc:localization).
 */
// English is the default. French is built in. Regional tags fall back to their
// registered language (fr-CA -> fr). Unknown languages leave the locale unchanged.
CYUI_API bool setLocale(const char* code);
CYUI_API const std::string& locale();
CYUI_API std::vector<LocaleInfo> availableLocales();
// Merge host strings or add another language. Call outside concurrent rendering.
CYUI_API bool registerTranslations(const char* code, const TranslationCatalog&, const char* name = nullptr);
// English messages can be keys; applications may also use namespaced keys with
// an explicit English fallback. Missing translations always keep readable text.
CYUI_API std::string tr(const char* key, const char* englishFallback = nullptr);
// For interactive labels/window titles: translated text plus a stable ImGui ID.
// The identity is based on the English label and survives a live language change.
CYUI_API std::string trLabel(const char* key, const char* englishFallback = nullptr);
} // namespace cyui
