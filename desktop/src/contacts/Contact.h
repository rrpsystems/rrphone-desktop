#pragma once

#include <QString>

// D-16 — one <contact> row from the remote XML phonebook.
// Field set and names mirror the MicroSip format on purpose (Docs/RRP_Softphone_Desktop_PRD_Lite_v1.0.md,
// Section 8) so an existing MicroSip-compatible XML can be pointed at this
// app without any conversion.
struct Contact {
    QString name;
    QString number;   // dialed on click/double-click
    QString firstName;
    QString lastName;
    QString phone;    // secondary number, display-only
    QString mobile;   // secondary number, display-only
    QString email;
    QString address;
    QString city;
    QString state;
    QString zip;
    QString comment;
    QString info;     // free text, shown as secondary info (e.g. company/department)
    bool presence = false; // parsed but unused in the MVP — no presence channel yet
};
