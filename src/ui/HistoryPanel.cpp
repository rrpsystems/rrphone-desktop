#include "HistoryPanel.h"
#include "Theme.h"

#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QVBoxLayout>

namespace {
constexpr int RoleNumber = Qt::UserRole;

QString formatDuration(int seconds) {
    const int minutes = seconds / 60;
    return QStringLiteral("%1:%2")
        .arg(minutes, 2, 10, QLatin1Char('0'))
        .arg(seconds % 60, 2, 10, QLatin1Char('0'));
}
} // namespace

HistoryPanel::HistoryPanel(QWidget *parent) : QWidget(parent) {
    m_listWidget = new QListWidget(this);
    connect(m_listWidget, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem *item) {
        const QString number = item->data(RoleNumber).toString();
        if (!number.isEmpty()) {
            emit callRequested(number);
        }
    });

    auto *clearButton = new QPushButton(tr("Limpar histórico"), this);
    connect(clearButton, &QPushButton::clicked, this, [this]() {
        const auto answer = QMessageBox::question(this, tr("Limpar histórico"),
                                                   tr("Apagar todas as chamadas registradas?"),
                                                   QMessageBox::Yes | QMessageBox::No);
        if (answer == QMessageBox::Yes) {
            CallHistoryStore::clear();
            reload();
        }
    });

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(6);
    layout->addWidget(m_listWidget, 1);
    layout->addWidget(clearButton);

    reload();
}

void HistoryPanel::reload() {
    m_listWidget->clear();

    const QList<CallRecord> records = CallHistoryStore::load();
    if (records.isEmpty()) {
        auto *empty = new QListWidgetItem(tr("Nenhuma chamada registrada ainda."), m_listWidget);
        empty->setFlags(Qt::NoItemFlags);
        return;
    }

    for (const CallRecord &record : records) {
        // Arrow shows the direction, colour shows whether it connected —
        // a missed call has to be findable at a glance.
        QString arrow;
        const char *color = Theme::kTextSecondary;
        if (record.incoming && record.answered) {
            arrow = QStringLiteral("↙");
            color = Theme::kAccentTeal;
        } else if (record.incoming) {
            arrow = QStringLiteral("✖");
            color = Theme::kDangerRed;
        } else {
            arrow = QStringLiteral("↗");
            color = record.answered ? Theme::kAccentBlue : Theme::kTextSecondary;
        }

        const QString who = record.displayName.isEmpty()
                                 ? record.peer
                                 : QStringLiteral("%1 (%2)").arg(record.displayName, record.peer);
        QString detail = record.startedAt.toString(QStringLiteral("dd/MM HH:mm"));
        if (record.answered) {
            detail += QStringLiteral(" · %1").arg(formatDuration(record.durationSeconds));
        }
        if (!record.note.isEmpty()) {
            detail += QStringLiteral(" · %1").arg(record.note);
        }

        auto *item = new QListWidgetItem(QStringLiteral("%1  %2\n%3").arg(arrow, who, detail), m_listWidget);
        item->setData(RoleNumber, record.peer);
        item->setForeground(QColor(color));
        item->setToolTip(tr("Duplo clique para ligar para %1").arg(record.peer));
    }
}
