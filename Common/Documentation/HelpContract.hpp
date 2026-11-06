// Copyright (C) 2026 Cyberalien. AGPL-3.0-or-later.
#pragma once
#include "../../ThirdParty/CyDoc/cydoc_link.hpp"
#include <string>
/**
 * @doc help-contract
 * @locale en
 * @title Native help integration
 * @section API
 * @symbol cyhelp::Route
 * ## Local route {#route}
 * The host converts its installed Documentation.html path to a file URL. Route keeps
 * stable page, section and media identifiers encoded through CyDoc's C++ helper.
 * English is the default; unsupported requested locales fall back to English.
 * The legacy --manual-topic, --manual-language and --manual-media arguments
 * continue to address this reader. No server or runtime Node.js is required.
 */
/**
 * @doc help-contract
 * @locale fr
 * @title Intégration de l’aide native
 * @section API
 * @symbol cyhelp::Route
 * ## Adresse locale {#route}
 * L’application convertit le chemin installé Documentation.html en URL file://. Route
 * encode les identifiants stables de page, section et média avec l’aide C++ de
 * CyDoc. L’anglais est la langue par défaut et de repli. Les arguments historiques
 * --manual-topic, --manual-language et --manual-media restent pris en charge.
 * Aucun serveur ni Node.js n’est nécessaire à la lecture.
 */
namespace cyhelp {
inline std::string Route(const std::string& base, const std::string& locale,
                         const std::string& topic, const std::string& anchor={},
                         const std::string& media={}) {
    return cydoc_native::link(base,locale=="fr"?"fr":"en",topic.empty()?"welcome":topic,anchor,
        media.empty()?std::vector<std::pair<std::string,std::string>>{}:
                      std::vector<std::pair<std::string,std::string>>{{"media",media}});
}
}
