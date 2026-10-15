#ifndef UPDATEGROUPING_H
#define UPDATEGROUPING_H

#include <QString>
#include <QStringList>
#include <QList>

/*
 * Grouping table for the software-update feature.
 *
 * Calaos packages ("calaos-*") are never grouped: they are always shown
 * individually in UpdatePackagesModel. All other (system) packages reported
 * by the package manager are bucketed into synthetic groups (UpdateGroupsModel)
 * so the UI can show a single line (e.g. "kernel", "system") instead of
 * hundreds of unrelated package rows.
 *
 * This table is intentionally header-only so the rules stay easy to find
 * and grep for. "system" is the implicit fallback for anything no rule
 * matches: new rules can be inserted anywhere in the table.
 */
namespace UpdateGrouping {

struct Rule
{
    QString groupId;
    QStringList prefixes;
    QStringList exactNames;
};

// Ordered rules; first match wins; anything unmatched falls back to "system".
inline const QList<Rule> &rules()
{
    static const QList<Rule> ruleList = {
        {
            "kernel",
            {
                "linux-image",
                "linux-headers",
                "linux-firmware",
                "linux-libc",
                "linux-kbuild",
                "linux-compiler",
            },
            {
                "firmware-linux",
                "firmware-linux-nonfree",
                "intel-microcode",
                "amd64-microcode",
            }
        },
    };

    return ruleList;
}

inline QString groupForPackage(const QString &name)
{
    //calaos packages are never grouped, they are always shown individually
    if (name.startsWith("calaos-"))
        return QString();

    for (const Rule &rule: rules())
    {
        for (const QString &prefix: rule.prefixes)
        {
            if (name.startsWith(prefix))
                return rule.groupId;
        }

        for (const QString &exact: rule.exactNames)
        {
            if (name == exact)
                return rule.groupId;
        }
    }

    //fallback: anything not matched by a specific rule goes to "system"
    return QStringLiteral("system");
}

} // namespace UpdateGrouping

#endif // UPDATEGROUPING_H
