#pragma once

/**
 * @doc api-core
 * @locale en
 * @title Core entry point
 * @section API
 * @symbol cyui::Core
 * The dependency-light umbrella for themes, localization, filters, widgets and layout.
 *
 * ## Usage {#usage}
 * Include `<cyui/Core.h>` and link `CyUI::Core`. Initialize the current ImGui context
 * with `apply` and `loadFonts` before its first frame. Use palette roles and `dp`
 * for custom drawing. Optional modules remain separate targets.
 * [Startup recipe](doc:integration#startup). [Widget reference](doc:widgets).
 */
/**
 * @doc api-core
 * @locale fr
 * @title Point d’entrée Core
 * @section API
 * @symbol cyui::Core
 * L’en-tête léger des thèmes, de la localisation, des filtres, widgets et dispositions.
 *
 * ## Utilisation {#usage}
 * Incluez `<cyui/Core.h>` et liez `CyUI::Core`. Initialisez le contexte ImGui courant
 * avec `apply` et `loadFonts` avant sa première frame. Utilisez les rôles de palette
 * et `dp` pour vos dessins. Les modules optionnels restent des cibles distinctes.
 * [Initialisation](doc:integration#startup). [Référence des widgets](doc:widgets).
 */

// The shared CyUI foundation. Link CyUI::Core; optional modules stay separate.
#include "cyui/Theme.h"
#include "cyui/Locale.h"
#include "cyui/Filter.h"
#include "cyui/Icons.h"
#include "cyui/CoreWidgets.h"
#include "cyui/Layout.h"
#include "cyui/Numeric.h"
