#include "ui/file_preview_view.hpp"
#include "config/files_config.hpp"
#include <miqutoolkit/view/button.hpp>
#include <miqutoolkit/view/list_item_view.hpp>
#include <miqutoolkit/core/config.hpp>
#include <fstream>
#include <sstream>

namespace miqu {

FilePreviewView::FilePreviewView() {
    LayoutParams lp;
    lp.width = static_cast<int>(LayoutDimension::MatchParent);
    lp.height = static_cast<int>(LayoutDimension::MatchParent);
    set_layout_params(lp);

    show_empty_state();
}

void FilePreviewView::update_preview(const FileItem* item, bool show_hidden) {
    clear_views();
    m_current_content = nullptr;

    if (!item) {
        show_empty_state("No item selected");
        return;
    }

    if (item->is_dir) {
        show_directory_preview(*item, show_hidden);
    } else if (item->category == FileCategory::Image) {
        show_image_preview(*item);
    } else if (item->category == FileCategory::CodeText) {
        show_text_preview(*item);
    } else {
        show_binary_preview(*item);
    }

    request_redraw();
}

void FilePreviewView::show_empty_state(const std::string& message) {
    clear_views();

    auto config = Config::get();
    auto layout = std::make_shared<LinearLayout>(Orientation::Vertical);
    layout->set_divider_spacing(8);

    LayoutParams layout_lp;
    layout_lp.width = static_cast<int>(LayoutDimension::WrapContent);
    layout_lp.height = static_cast<int>(LayoutDimension::WrapContent);
    layout_lp.gravity = Gravity::Center;
    layout->set_layout_params(layout_lp);

    auto icon = std::make_shared<ImageView>("view-preview");
    icon->set_target_size(48);
    icon->set_dim(0.35f);
    LayoutParams icon_lp;
    icon_lp.width = 48;
    icon_lp.height = 48;
    icon_lp.gravity = Gravity::CenterHorizontal;
    layout->add_view(icon, icon_lp);

    auto text = TextViewBuilder::create()
        ->text(message)
        ->textSize(12)
        ->center(true)
        ->textColor(config->colors.outline)
        ->build();
    layout->add_view(text);

    add_view(layout);
    m_current_content = layout;
}

void FilePreviewView::show_directory_preview(const FileItem& item, bool show_hidden) {
    auto config = Config::get();
    auto root = std::make_shared<LinearLayout>(Orientation::Vertical);
    root->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::MatchParent)
    ));
    root->set_padding(8);
    root->set_divider_spacing(6);

    // Header label
    auto header = TextViewBuilder::create()
        ->text("Contents of " + item.name + " (" + item.size_formatted + ")")
        ->bold(true)
        ->textSize(11)
        ->textColor(config->colors.primary)
        ->build();
    root->add_view(header);

    auto children = FileItem::scan_directory(item.path, show_hidden);
    if (children.empty()) {
        auto empty_label = TextViewBuilder::create()
            ->text("Empty Folder")
            ->textSize(11)
            ->textColor(config->colors.outline)
            ->build();
        root->add_view(empty_label);
    } else {
        auto list = std::make_shared<ListView>();
        list->set_item_height(30);
        list->set_spacing(2);
        list->set_item_corner_radius(4);
        list->set_layout_params(LayoutParams(
            static_cast<int>(LayoutDimension::MatchParent),
            static_cast<int>(LayoutDimension::MatchParent)
        ));

        for (const auto& ch : children) {
            auto item_view = ListItemViewBuilder::create()
                ->title(ch.name)
                ->trailingText(ch.size_formatted)
                ->iconSource(ch.icon_name)
                ->iconSize(18)
                ->build();
            list->add_item(item_view);
        }

        root->add_view(list);
    }

    add_view(root);
    m_current_content = root;
}

void FilePreviewView::show_image_preview(const FileItem& item) {
    auto config = Config::get();
    auto root = std::make_shared<LinearLayout>(Orientation::Vertical);
    root->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::MatchParent)
    ));
    root->set_padding(12);
    root->set_divider_spacing(8);

    // Image container card
    auto img_card = CardViewBuilder::create()
        ->backgroundColor(config->colors.surface_variant)
        ->cornerRadius(config->metrics.corner_radius)
        ->stroke(1, config->colors.outline_variant)
        ->elevation(0)
        ->build();
    LayoutParams card_lp;
    card_lp.width = static_cast<int>(LayoutDimension::MatchParent);
    card_lp.height = 0;
    card_lp.weight = 1.0f;
    img_card->set_layout_params(card_lp);

    auto img = std::make_shared<ImageView>(item.path.string());
    img->set_fit_mode(FitMode::Contain);
    img->set_quality_mode(ImageQuality::FullOriginal);
    img->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::MatchParent)
    ));
    img_card->add_view(img);
    root->add_view(img_card);

    // Info metadata
    auto title = TextViewBuilder::create()
        ->text(item.name)
        ->bold(true)
        ->textSize(12)
        ->build();
    root->add_view(title);

    std::string meta_str = item.size_formatted + " • " + item.mtime_formatted + " • " + item.permissions_str;
    auto meta = TextViewBuilder::create()
        ->text(meta_str)
        ->textSize(10)
        ->textColor(config->colors.outline)
        ->build();
    root->add_view(meta);

    // Open button
    auto btn_open = ButtonBuilder::create()
        ->text("Open Image")
        ->style(ButtonStyle::Primary)
        ->onClick([this, p = item.path]() {
            if (m_on_open) m_on_open(p);
        })
        ->build();
    LayoutParams btn_lp;
    btn_lp.width = static_cast<int>(LayoutDimension::WrapContent);
    btn_lp.height = 32;
    btn_open->set_layout_params(btn_lp);
    root->add_view(btn_open);

    add_view(root);
    m_current_content = root;
}

void FilePreviewView::show_text_preview(const FileItem& item) {
    auto config = Config::get();
    auto& fcfg = FilesConfig::get();

    // Check file size threshold before attempting preview
    if (item.size_bytes > fcfg.preview_max_file_size_kb * 1024) {
        show_binary_preview(item);
        return;
    }

    std::ifstream file(item.path);
    if (!file.is_open()) {
        show_binary_preview(item);
        return;
    }

    std::string content;
    std::string line;
    size_t line_count = 0;
    while (std::getline(file, line) && line_count < fcfg.preview_max_text_lines) {
        // Line number prefix
        std::string num_str = std::to_string(line_count + 1);
        while (num_str.size() < 4) num_str = " " + num_str;
        content += num_str + " │ " + line + "\n";
        line_count++;
    }

    auto root = std::make_shared<LinearLayout>(Orientation::Vertical);
    root->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::MatchParent)
    ));
    root->set_padding(8);
    root->set_divider_spacing(6);

    // Header info
    std::string header_text = item.name + " (" + std::to_string(line_count) + " lines, " + item.size_formatted + ")";
    auto header = TextViewBuilder::create()
        ->text(header_text)
        ->bold(true)
        ->textSize(11)
        ->textColor(config->colors.primary)
        ->build();
    root->add_view(header);

    // Code card with ScrollView
    auto code_card = CardViewBuilder::create()
        ->backgroundColor(config->colors.surface_variant)
        ->cornerRadius(config->metrics.corner_radius)
        ->stroke(1, config->colors.outline_variant)
        ->elevation(0)
        ->padding(8)
        ->build();
    LayoutParams card_lp;
    card_lp.width = static_cast<int>(LayoutDimension::MatchParent);
    card_lp.height = 0;
    card_lp.weight = 1.0f;
    code_card->set_layout_params(card_lp);

    auto text_view = TextViewBuilder::create()
        ->text(content)
        ->fontFamily("Monospace")
        ->textSize(10)
        ->wrap(false)
        ->build();

    auto scroll = ScrollViewBuilder::create()
        ->contentView(text_view)
        ->build();
    scroll->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::MatchParent)
    ));

    code_card->add_view(scroll);
    root->add_view(code_card);

    add_view(root);
    m_current_content = root;
}

void FilePreviewView::show_binary_preview(const FileItem& item) {
    auto config = Config::get();
    auto root = std::make_shared<LinearLayout>(Orientation::Vertical);
    root->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::MatchParent)
    ));
    root->set_padding(16);
    root->set_divider_spacing(12);

    // Centered big icon
    auto icon_layout = std::make_shared<LinearLayout>(Orientation::Vertical);
    icon_layout->set_divider_spacing(6);
    LayoutParams icon_lp;
    icon_lp.width = static_cast<int>(LayoutDimension::MatchParent);
    icon_lp.height = static_cast<int>(LayoutDimension::WrapContent);
    icon_lp.gravity = Gravity::CenterHorizontal;
    icon_layout->set_layout_params(icon_lp);

    auto icon = std::make_shared<ImageView>(item.icon_name);
    icon->set_target_size(64);
    LayoutParams img_lp;
    img_lp.width = 64;
    img_lp.height = 64;
    img_lp.gravity = Gravity::CenterHorizontal;
    icon_layout->add_view(icon, img_lp);

    auto title = TextViewBuilder::create()
        ->text(item.name)
        ->bold(true)
        ->textSize(13)
        ->center(true)
        ->build();
    icon_layout->add_view(title);

    auto cat_badge = TextViewBuilder::create()
        ->text(item.category_label)
        ->textSize(10)
        ->center(true)
        ->textColor(config->colors.primary)
        ->build();
    icon_layout->add_view(cat_badge);

    root->add_view(icon_layout);

    // Metadata card
    auto meta_card = CardViewBuilder::create()
        ->backgroundColor(config->colors.surface_variant)
        ->cornerRadius(config->metrics.corner_radius)
        ->stroke(1, config->colors.outline_variant)
        ->padding(12)
        ->build();
    LayoutParams meta_lp;
    meta_lp.width = static_cast<int>(LayoutDimension::MatchParent);
    meta_lp.height = static_cast<int>(LayoutDimension::WrapContent);
    meta_card->set_layout_params(meta_lp);

    auto meta_list = std::make_shared<LinearLayout>(Orientation::Vertical);
    meta_list->set_divider_spacing(6);

    std::string s_size = "Size: " + item.size_formatted + " (" + std::to_string(item.size_bytes) + " bytes)";
    std::string s_perm = "Permissions: " + item.permissions_str;
    std::string s_time = "Modified: " + item.mtime_formatted;
    std::string s_path = "Location: " + item.path.parent_path().string();

    for (const auto& line : {s_size, s_perm, s_time, s_path}) {
        auto tv = TextViewBuilder::create()
            ->text(line)
            ->textSize(10)
            ->textColor(config->colors.on_surface_variant)
            ->build();
        meta_list->add_view(tv);
    }
    meta_card->add_view(meta_list);
    root->add_view(meta_card);

    // Open button
    auto btn_open = ButtonBuilder::create()
        ->text("Open with Default Application")
        ->style(ButtonStyle::Primary)
        ->onClick([this, p = item.path]() {
            if (m_on_open) m_on_open(p);
        })
        ->build();
    LayoutParams btn_lp;
    btn_lp.width = static_cast<int>(LayoutDimension::MatchParent);
    btn_lp.height = 36;
    btn_open->set_layout_params(btn_lp);
    root->add_view(btn_open);

    add_view(root);
    m_current_content = root;
}

} // namespace miqu
