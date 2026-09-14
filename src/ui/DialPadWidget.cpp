#include "DialPadWidget.h"
#include "DialKeyButton.h"

#include <QGridLayout>

namespace {
struct KeyDef {
    QChar digit;
    const char *letters;
};

const KeyDef kKeys[4][3] = {
    {{'1', ""}, {'2', "ABC"}, {'3', "DEF"}},
    {{'4', "GHI"}, {'5', "JKL"}, {'6', "MNO"}},
    {{'7', "PQRS"}, {'8', "TUV"}, {'9', "WXYZ"}},
    {{'*', ""}, {'0', "+"}, {'#', ""}},
};
} // namespace

DialPadWidget::DialPadWidget(QWidget *parent) : QWidget(parent) {
    auto *grid = new QGridLayout(this);
    grid->setContentsMargins(6, 4, 6, 4);
    grid->setSpacing(4);

    for (int row = 0; row < 4; ++row) {
        for (int col = 0; col < 3; ++col) {
            const KeyDef &key = kKeys[row][col];
            auto *button = new DialKeyButton(key.digit, QString::fromLatin1(key.letters), this);
            button->setMinimumSize(62, 38);
            connect(button, &DialKeyButton::clicked, this, [this, button]() {
                emit digitPressed(button->digit());
            });
            grid->addWidget(button, row, col);
        }
    }
}
