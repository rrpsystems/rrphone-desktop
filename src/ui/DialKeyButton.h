#pragma once

#include <QPushButton>

// A keypad key drawn like the Android app's: a pill with the digit on top and
// the small letter group underneath ("2 / ABC", "0 / +"). Painted by hand —
// see paintEvent().
class DialKeyButton : public QPushButton {
    Q_OBJECT

public:
    DialKeyButton(QChar digit, const QString &letters, QWidget *parent = nullptr);

    QChar digit() const { return m_digit; }

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QChar m_digit;
    QString m_letters;
};
