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
            border: none;
            border-radius: 8px;
            padding: 6px 12px;
        }
        QPushButton:hover { background-color: %5; }
        QPushButton:pressed { background-color: %6; }
        QPushButton:disabled { color: %8; background-color: %6; }

        /* Call actions: circles with the label underneath, like the Android
           call screen. Size comes from Theme::kRoundActionSize. */
        QPushButton#round {
            border-radius: 21px;
            padding: 0;
            min-width: 42px; max-width: 42px;
            min-height: 42px; max-height: 42px;
        }
        QPushButton#round:checked { background-color: %7; }
        QPushButton#round:disabled { background-color: %6; }
        QLabel#roundLabel { color: %2; font-size: 11px; }
        QLabel#roundLabel:disabled { color: %8; }

        /* Flat icon buttons (hook line) */
        QToolButton {
            background-color: transparent;
            border: none;
            border-radius: 8px;
            padding: 4px;
        }
        QToolButton:hover { background-color: %5; }
        QToolButton:pressed { background-color: %6; }
        QToolButton:checked { background-color: %7; }
        QToolButton:disabled { background-color: transparent; }
        /* The settings gear opens a menu; Qt's little arrow overlaps the icon. */
        QToolButton::menu-indicator { image: none; width: 0; }

        /* Bottom navigation: icon over label, selected item on a soft pill. */
        QToolButton#nav {
            color: %8;
            font-size: 10px;
            border-radius: 12px;
            padding: 3px 10px;
        }
        QToolButton#nav:checked { background-color: %5; color: %2; }
        QToolButton#nav:hover { color: %2; }

        QLineEdit {
            background-color: %6;
            border: 1px solid %4;
            border-radius: 8px;
            padding: 6px;
            color: %2;
            selection-background-color: %7;
        }
        QLineEdit:focus { border-color: %7; }
        QListWidget, QListView, QTreeView {
            background-color: %6;
            border: 1px solid %4;
            border-radius: 8px;
        }
        QListWidget::item { padding: 6px 4px; border-radius: 6px; }
        QListWidget::item:selected { background-color: %7; }
        QListWidget#codecList::item { padding: 1px 4px; min-height: 20px; }

        QMenu { background-color: %6; color: %2; border: 1px solid %4; border-radius: 8px; }
        QMenu::item { padding: 6px 22px; }
        QMenu::item:selected { background-color: %7; }

        QComboBox {
            background-color: %6;
            border: 1px solid %4;
            border-radius: 8px;
            padding: 5px 8px;
        }
        QComboBox QAbstractItemView {
            background-color: %6;
            border: 1px solid %4;
            selection-background-color: %7;
        }
        QCheckBox, QLabel { background: transparent; }

        /* Settings: titled cards on a scrolling page, as in the Android app. */
        QScrollArea, QScrollArea > QWidget > QWidget#scrollContent { background: %1; border: none; }
        QFrame#card { background-color: %6; border-radius: 12px; }
        QFrame#card QLabel, QFrame#card QCheckBox { background: transparent; }
        QFrame#card QLineEdit, QFrame#card QComboBox, QFrame#card QListWidget { background-color: %1; }
        QLabel#cardTitle { color: %7; font-size: 13px; font-weight: 600; }
        QLabel#hint { color: %8; font-size: 11px; }

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

        QScrollBar:vertical { background: transparent; width: 9px; margin: 0; }
        QScrollBar::handle:vertical { background: %4; border-radius: 4px; min-height: 24px; }
        QScrollBar::add-line, QScrollBar::sub-line { height: 0; }
        QScrollBar::add-page, QScrollBar::sub-page { background: transparent; }
    )")
        .arg(kBackground, kTextPrimary, kButton, kBorder, kButtonHover, kPanel, kAccentBlue, kTextSecondary);
}
