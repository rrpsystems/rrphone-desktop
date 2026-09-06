#include "DialKeyButton.h"
#include "Theme.h"

#include <QPainter>
#include <QFontMetrics>
#include <QStyleOptionButton>
#include <QStyle>

DialKeyButton::DialKeyButton(QChar digit, const QString &letters, QWidget *parent)
    : QPushButton(parent), m_digit(digit), m_letters(letters) {
    setFocusPolicy(Qt::NoFocus);
    // The label is painted by hand, so the button carries no text of its own
    // — without this the key is nameless to screen readers (and to UI
    // automation).
    setAccessibleName(QString(digit));
}

void DialKeyButton::paintEvent(QPaintEvent *event) {
    // Let the stylesheet draw the background/border/hover states, then paint
    // our own two-size label on top of it.
    QPushButton::paintEvent(event);

    QPainter painter(this);
    painter.setRenderHint(QPainter::TextAntialiasing);

    QFont digitFont = font();
    digitFont.setPointSizeF(15.0);
    digitFont.setWeight(QFont::Medium);
    const QFontMetrics digitMetrics(digitFont);

    QFont lettersFont = font();
    lettersFont.setPointSizeF(7.5);
    const QFontMetrics lettersMetrics(lettersFont);

    const QString digitText(m_digit);
    const int digitWidth = digitMetrics.horizontalAdvance(digitText);
    const int lettersWidth = m_letters.isEmpty() ? 0 : lettersMetrics.horizontalAdvance(m_letters) + 3;

    const int startX = (width() - digitWidth - lettersWidth) / 2;
    const int baseline = height() / 2 + digitMetrics.capHeight() / 2;

    painter.setFont(digitFont);
    painter.setPen(QColor(Theme::kTextPrimary));
    painter.drawText(startX, baseline, digitText);

    if (!m_letters.isEmpty()) {
        painter.setFont(lettersFont);
        painter.setPen(QColor(Theme::kTextSecondary));
        painter.drawText(startX + digitWidth + 3, baseline, m_letters);
    }
}
