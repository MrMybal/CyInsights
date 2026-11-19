#pragma once
/**
 * @doc api-rich-document
 * @locale en
 * @title Paginated native documents
 * @section API
 * @symbol cyui::RichDocumentEditor
 * Link `CyUI::RichDocument` and keep the editor between frames. Set a `RichDocumentData`, then call `draw` (bool changed). Native block editing, page settings, headers/footers and tables share document undo. Host fonts determine pagination; there is no continuous caret across pages.
 *
 * ## Usage {#usage}
 * [Module guide](doc:office#writer).
 */
/**
 * @doc api-rich-document
 * @locale fr
 * @title Documents natifs paginés
 * @section API
 * @symbol cyui::RichDocumentEditor
 * Liez `CyUI::RichDocument` et conservez l’éditeur entre les frames. Chargez un `RichDocumentData`, puis appelez `draw` (booléen de modification). Blocs natifs, mise en page, en-têtes/pieds et tableaux partagent l’annulation. La police hôte détermine la pagination ; il n’y a pas de curseur continu entre les pages.
 *
 * ## Utilisation {#usage}
 * [Guide du module](doc:office#writer).
 */
#include "cyui/modules/DocumentEditor.h"
#include "cyui/modules/DataGrid.h"
namespace cyui {
enum class RichBlockKind { paragraph, table, image, pageBreak };
struct RichParagraphStyle {
    float points = 11, lineSpacing = 1.2f, before = 0, after = 6, indent = 0;
    GridAlignment alignment = GridAlignment::left;
    int heading = 0;
};
struct RichBlock {
    RichBlockKind kind = RichBlockKind::paragraph;
    std::vector<DocumentInline> runs;
    RichParagraphStyle paragraph;
    GridModel table{2, 3};
    std::string image, caption;
    float imageWidthMM = 100, imageHeightMM = 65;
};
struct RichPageSettings {
    float widthMM = 210, heightMM = 297;
    float leftMM = 20, rightMM = 20, topMM = 20, bottomMM = 20;
    std::string header, footer; // {page} and {pages} expand in the native view
};
struct RichDocumentData {
    RichPageSettings page;
    std::vector<RichBlock> blocks;
    std::vector<DocumentAsset> assets;
};
// Layout uses physical page dimensions. Fragment offsets refer to UTF-8 run text.
struct RichPageFragment {
    std::size_t block = 0, first = 0, last = 0;
    float y = 0, height = 0;
    bool tableRows = false;
};
struct RichPage {
    std::vector<RichPageFragment> fragments;
};
CYUI_API void validateRichDocument(const RichDocumentData &);
CYUI_API std::vector<RichPage> layoutRichDocument(const RichDocumentData &,
                                                  float logicalPixelsPerMM = 96.f / 25.4f);
CYUI_API std::string richDocumentMarkdown(const RichDocumentData &);
CYUI_API RichDocumentData richDocumentFromMarkdown(const std::string &);
struct RichDocumentOptions {
    bool readOnly = false, toolbar = true;
    const DocumentServices *services = nullptr;
    std::function<RichBlock()> importImage;
};
class CYUI_API RichDocumentEditor {
  public:
    RichDocumentEditor();
    ~RichDocumentEditor();
    RichDocumentEditor(const RichDocumentEditor &) = delete;
    RichDocumentEditor &operator=(const RichDocumentEditor &) = delete;
    void setDocument(const RichDocumentData &);
    const RichDocumentData &document() const;
    bool undo();
    bool redo();
    std::size_t selectedBlock() const;
    bool selectBlock(std::size_t);
    bool editBlock(std::size_t);
    void setZoom(float);
    bool draw(const char *id, ImVec2 size = {}, const RichDocumentOptions & = {});

  private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
} // namespace cyui
