#include "Theme.h"

QString Theme::styleSheet() {
    return QStringLiteral(R"(
        QWidget {
            background-color: %1;
            color: %2;
            font-family: 'Segoe UI';
            font-size: 12px;
        }
        QMainWindow, QDialog { background-color: %1; }

        QPushButton {
            background-color: %3;
            color: %2;
            border: 1px solid %4;
            border-radius: 4px;
            padding: 5px;
        }
        QPushButton:hover { background-color: %5; }
        QPushButton:pressed { background-color: %6; }
        QPushButton:disabled { color: %8; border-color: %6; }

        /* Flat icon buttons (action row + bottom bar) */
        QToolButton {
            background-color: transparent;
            border: none;
            border-radius: 4px;
            padding: 4px;
        }
        QToolButton:hover { background-color: %5; }
        QToolButton:pressed { background-color: %6; }
        QToolButton:checked { background-color: %7; }
        QToolButton:disabled { background-color: transparent; }
        /* The settings gear opens a menu; Qt's little arrow overlaps the icon. */
        QToolButton::menu-indicator { image: none; width: 0; }

        QLineEdit {
            background-color: %6;
            border: 1px solid %4;
            border-radius: 4px;
            padding: 5px;
            color: %2;
            selection-background-color: %7;
        }
        QListWidget, QListView, QTreeView {
            background-color: %6;
            border: 1px solid %4;
        }
        QListWidget::item { padding: 5px 4px; }
        QListWidget::item:selected { background-color: %7; }

        QMenu { background-color: %6; color: %2; border: 1px solid %4; }
        QMenu::item { padding: 6px 22px; }
        QMenu::item:selected { background-color: %7; }

        QTabWidget::pane { border: 1px solid %4; }
        QTabBar::tab { background: %6; color: %8; padding: 6px 12px; }
        QTabBar::tab:selected { background: %3; color: %2; }

        QComboBox {
            background-color: %6;
            border: 1px solid %4;
            border-radius: 4px;
            padding: 4px;
        }
        QComboBox QAbstractItemView {
            background-color: %6;
            border: 1px solid %4;
            selection-background-color: %7;
        }
        QCheckBox, QLabel { background: transparent; }

        QSlider::groove:horizontal {
            background: %4;
            height: 3px;
            border-radius: 2px;
        }
        QSlider::sub-page:horizontal {
            background: %7;
            height: 3px;
            border-radius: 2px;
        }
        QSlider::handle:horizontal {
            background: %2;
            border: none;
            width: 13px;
            margin: -5px 0;
            border-radius: 6px;
        }
        QSlider::handle:horizontal:hover { background: %7; }

        QScrollBar:vertical { background: %1; width: 9px; margin: 0; }
        QScrollBar::handle:vertical { background: %4; border-radius: 4px; min-height: 24px; }
        QScrollBar::add-line, QScrollBar::sub-line { height: 0; }
    )")
        .arg(kBackground, kTextPrimary, kButton, kBorder, kButtonHover, kPanel, kAccentBlue, kTextSecondary);
}
