// Copyright (C) 2026 Cyberalien. AGPL-3.0-or-later.
#pragma once
#include "Application.hpp"
namespace cyinsights {
// Documents own their analysis state. Detaching changes only its native host.
class DocumentWorkspace {
public:
    explicit DocumentWorkspace(ImTextureID logo);
    Application& Primary(){return *documents_.front().app;}
    void Open(const fs::path& path);
    void Draw();
    bool Busy() const;
    bool CanUpdate() const;
    std::vector<fs::path> OpenPaths() const;
    void Detach(size_t index);
    void Attach(size_t index);
    void Select(size_t index);
    size_t Count() const {return documents_.size();}
    Application& At(size_t index){return *documents_.at(index).app;}
    void CloseCapture(size_t index){Close(index);}
    void WriteReport(const fs::path& file) const;
private:
    struct Document {uint64_t id=0;std::unique_ptr<Application> app;bool detached=false,position=false;ImGuiID viewport=0;};
    Document& Create();
    void Close(size_t index);
    std::string Title(const Document& doc) const;
    ImTextureID logo_=0;
    uint64_t next_id_=0,selected_=0,menu_owner_=0;
    bool select_pending_=true;
    std::vector<Document> documents_;
    std::vector<fs::path> pending_open_;
    std::vector<std::future<void>> closing_;
};
}
