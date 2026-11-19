#pragma once
/**
 * @doc api-office-io
 * @locale en
 * @title Office document exchange
 * @section API
 * @symbol cyui::readOfficeDocument
 * Link `CyUI::OfficeIO` to read/write ODT/DOCX, ODS/XLSX and flat FODT/FODS from memory. Packaged image bytes are returned for native host loading. Inspect `OfficeIssue` before conversion; strict mode rejects recognized losses. This is a documented subset, not arbitrary-file lossless editing.
 *
 * ## Usage {#usage}
 * [Module guide](doc:office#formats).
 */
/**
 * @doc api-office-io
 * @locale fr
 * @title Échange de documents bureautiques
 * @section API
 * @symbol cyui::readOfficeDocument
 * Liez `CyUI::OfficeIO` pour lire/écrire ODT/DOCX, ODS/XLSX et FODT/FODS en mémoire. Les images intégrées sont retournées pour chargement natif par l’hôte. Consultez `OfficeIssue` ; le mode strict refuse les pertes reconnues. Ce sous-ensemble documenté ne garantit pas la conservation de tout fichier arbitraire.
 *
 * ## Utilisation {#usage}
 * [Guide du module](doc:office#formats).
 */
#include "cyui/modules/RichDocument.h"
#include "cyui/modules/Spreadsheet.h"
#include "cyui/modules/DocumentIO.h"
namespace cyui {
enum class OfficeFormat { odt, docx, ods, xlsx, fodt, fods };
struct OfficeIssue {
    std::string part, feature, message;
};
struct OfficeDocument {
    RichDocumentData document;
    std::vector<OfficeIssue> issues;
};
struct OfficeWorkbook {
    Workbook workbook;
    std::vector<OfficeIssue> issues;
};
struct OfficeOptions {
    DocumentIOLimits limits;
    // Refuse lossy imports/exports instead of returning issues for unsupported features.
    bool strict = false;
};
CYUI_API OfficeDocument readOfficeDocument(ArrayView<unsigned char>, OfficeFormat,
                                           const OfficeOptions & = {});
CYUI_API std::vector<unsigned char> writeOfficeDocument(const RichDocumentData &, OfficeFormat,
                                                        std::vector<OfficeIssue> *issues = nullptr,
                                                        const OfficeOptions & = {});
CYUI_API OfficeWorkbook readOfficeWorkbook(ArrayView<unsigned char>, OfficeFormat,
                                           const OfficeOptions & = {});
CYUI_API std::vector<unsigned char> writeOfficeWorkbook(const Workbook &, OfficeFormat,
                                                        std::vector<OfficeIssue> *issues = nullptr,
                                                        const OfficeOptions & = {});
CYUI_API bool officeFormatFromExtension(const std::string &, OfficeFormat &);
} // namespace cyui
