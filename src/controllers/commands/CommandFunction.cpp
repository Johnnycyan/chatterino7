// SPDX-FileCopyrightText: 2024 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#include "controllers/commands/CommandFunction.hpp"

#include "common/Channel.hpp"
#include "messages/Message.hpp"
#include "messages/MessageFlag.hpp"
#include "providers/twitch/TwitchBadge.hpp"
#include "providers/twitch/TwitchChannel.hpp"

#include <QRegularExpression>
#include <QString>
#include <QStringList>

#include <algorithm>
#include <optional>

namespace chatterino {
namespace commands {

namespace {

// ─────────────────────────────────────────────
// Argument parser
// ─────────────────────────────────────────────

/// Split @a argStr by top-level commas (not inside nested parentheses).
/// Each part is trimmed of surrounding whitespace.
QStringList splitArgs(const QString &argStr)
{
    QStringList args;
    int depth = 0;
    int start = 0;
    const int n = argStr.length();

    for (int i = 0; i < n; ++i)
    {
        const QChar c = argStr[i];
        if (c == QLatin1Char('('))
        {
            ++depth;
        }
        else if (c == QLatin1Char(')'))
        {
            --depth;
        }
        else if (c == QLatin1Char(',') && depth == 0)
        {
            args.append(argStr.mid(start, i - start).trimmed());
            start = i + 1;
        }
    }

    // Append the last (or only) segment
    const QString last = argStr.mid(start).trimmed();
    if (!last.isEmpty() || !args.isEmpty())
    {
        args.append(last);
    }

    return args;
}

// ─────────────────────────────────────────────
// Helpers for $is()
// ─────────────────────────────────────────────

/// Returns true if the message author carries the badge whose key equals
/// @a badgeKey (e.g. "moderator", "subscriber", "broadcaster"…).
bool hasBadge(const Message *message, const QString &badgeKey)
{
    if (message == nullptr)
    {
        return false;
    }
    for (const TwitchBadge &badge : message->twitchBadges)
    {
        if (badge.key_ == badgeKey)
        {
            return true;
        }
    }
    return false;
}

/// Evaluate one @a matcher against @a value and the current context.
bool evaluateMatcher(const QString &value, const QString &matcher,
                     const ChannelPtr &channel, const Message *message)
{
    // user:name  — message login name
    if (matcher.startsWith(QStringLiteral("user:"), Qt::CaseInsensitive))
    {
        if (message == nullptr)
        {
            return false;
        }
        const QString name = matcher.mid(5).trimmed();
        return message->loginName.compare(name, Qt::CaseInsensitive) == 0;
    }

    // chan:channel  — current channel name
    if (matcher.startsWith(QStringLiteral("chan:"), Qt::CaseInsensitive))
    {
        if (!channel)
        {
            return false;
        }
        const QString name = matcher.mid(5).trimmed();
        return channel->getName().compare(name, Qt::CaseInsensitive) == 0;
    }

    // status:flags  — one character per badge flag, all must be present
    if (matcher.startsWith(QStringLiteral("status:"), Qt::CaseInsensitive))
    {
        const QString flags = matcher.mid(7).trimmed();
        for (const QChar flag : flags)
        {
            bool has = false;
            switch (flag.toLatin1())
            {
                case 'b':
                    has = hasBadge(message, QStringLiteral("broadcaster"));
                    break;
                case 'm':
                    has = hasBadge(message, QStringLiteral("moderator"));
                    break;
                case 's':
                    has = hasBadge(message, QStringLiteral("subscriber"));
                    break;
                case 'v':
                    has = hasBadge(message, QStringLiteral("vip"));
                    break;
                case 't':
                    has = hasBadge(message, QStringLiteral("turbo"));
                    break;
                case 'p':
                    has = hasBadge(message, QStringLiteral("premium"));
                    break;
                default:
                    has = false;
                    break;
            }
            if (!has)
            {
                return false;
            }
        }
        return true;
    }

    // re:pattern  — regex match on value
    if (matcher.startsWith(QStringLiteral("re:"), Qt::CaseInsensitive))
    {
        const QRegularExpression rx(matcher.mid(3));
        if (!rx.isValid())
        {
            return false;
        }
        return rx.match(value).hasMatch();
    }

    // cs:text  — case-sensitive substring
    if (matcher.startsWith(QStringLiteral("cs:")))
    {
        return value.contains(matcher.mid(3), Qt::CaseSensitive);
    }

    // w:text  — whole-word match (case-insensitive)
    if (matcher.startsWith(QStringLiteral("w:"), Qt::CaseInsensitive))
    {
        const QString pattern = QStringLiteral("\\b") +
                                QRegularExpression::escape(matcher.mid(2)) +
                                QStringLiteral("\\b");
        const QRegularExpression rx(pattern,
                                    QRegularExpression::CaseInsensitiveOption);
        return rx.match(value).hasMatch();
    }

    // start:text  — prefix match (case-insensitive)
    if (matcher.startsWith(QStringLiteral("start:"), Qt::CaseInsensitive))
    {
        return value.startsWith(matcher.mid(6), Qt::CaseInsensitive);
    }

    // end:text  — suffix match (case-insensitive)
    if (matcher.startsWith(QStringLiteral("end:"), Qt::CaseInsensitive))
    {
        return value.endsWith(matcher.mid(4), Qt::CaseInsensitive);
    }

    // mystatus:flags  — check the *local user's* status in the current channel
    // Flags (case-insensitive): b=broadcaster m=moderator v=vip
    if (matcher.startsWith(QStringLiteral("mystatus:"), Qt::CaseInsensitive))
    {
        auto *tc = dynamic_cast<TwitchChannel *>(channel.get());
        if (tc == nullptr)
        {
            return false;
        }
        const QString flags = matcher.mid(9).trimmed().toLower();
        for (const QChar flag : flags)
        {
            bool has = false;
            switch (flag.toLatin1())
            {
                case 'b':
                    has = tc->isBroadcaster();
                    break;
                case 'm':
                    has = tc->isMod();
                    break;
                case 'v':
                    has = tc->isVip();
                    break;
                default:
                    has = false;
                    break;
            }
            if (!has)
            {
                return false;
            }
        }
        return true;
    }

    // config:option[|param]  — match against message flags / channel state.
    // Multiple options can be combined with comma inside the prefix.
    // For config:b|badgeKey[/version] and config:t|key[=value] the pipe-
    // separated argument follows directly after the keyword.
    if (matcher.startsWith(QStringLiteral("config:"), Qt::CaseInsensitive))
    {
        const QString rest = matcher.mid(7);
        // Support comma-separated options: config:info,any
        const QStringList options =
            rest.split(QLatin1Char(','), Qt::SkipEmptyParts);

        for (const QString &opt : options)
        {
            // config:b|badgeKey[/version]  — match a Twitch badge on the message
            if (opt.startsWith(QStringLiteral("b|"), Qt::CaseInsensitive))
            {
                if (message == nullptr)
                {
                    return false;
                }
                const QString badgeSpec = opt.mid(2);
                const int slash = badgeSpec.indexOf(QLatin1Char('/'));
                const QString badgeKey =
                    (slash == -1) ? badgeSpec : badgeSpec.left(slash);
                const QString badgeVersion =
                    (slash == -1) ? QString() : badgeSpec.mid(slash + 1);
                bool found = false;
                for (const TwitchBadge &badge : message->twitchBadges)
                {
                    if (badge.key_.compare(badgeKey, Qt::CaseInsensitive) ==
                        0)
                    {
                        if (badgeVersion.isEmpty() ||
                            badge.value_.compare(badgeVersion,
                                                 Qt::CaseInsensitive) == 0)
                        {
                            found = true;
                            break;
                        }
                    }
                }
                if (!found)
                {
                    return false;
                }
                continue;
            }

            // config:live[|title/game regex]  — stream must be live
            if (opt.startsWith(QStringLiteral("live"), Qt::CaseInsensitive))
            {
                const bool negate =
                    opt.startsWith(QStringLiteral("!live"), Qt::CaseInsensitive);
                const QString optName = negate ? opt.mid(5) : opt.mid(4);
                auto *tc = dynamic_cast<TwitchChannel *>(channel.get());
                if (tc == nullptr)
                {
                    return negate ? true : false;
                }
                const auto &status = tc->accessStreamStatus();
                const bool live = status->live;
                if (!optName.startsWith(QLatin1Char('|')))
                {
                    // plain config:live / config:!live
                    if (negate ? live : !live)
                    {
                        return false;
                    }
                }
                else
                {
                    // config:live|regex  — check title or game
                    if (!live)
                    {
                        return negate ? false : false;
                    }
                    const QString pattern = optName.mid(1);
                    const QRegularExpression rx(
                        pattern, QRegularExpression::CaseInsensitiveOption);
                    const bool titleMatch =
                        rx.isValid() && rx.match(status->title).hasMatch();
                    const bool gameMatch =
                        rx.isValid() && rx.match(status->game).hasMatch();
                    const bool matched = titleMatch || gameMatch;
                    if (negate ? matched : !matched)
                    {
                        return false;
                    }
                }
                continue;
            }

            if (message == nullptr)
            {
                return false;
            }

            // Map simple keyword options to MessageFlags
            struct FlagMapping {
                const char *keyword;
                MessageFlag flag;
            };
            static const FlagMapping FLAG_MAP[] = {
                {"info", MessageFlag::System},
                {"hl", MessageFlag::RedeemedHighlight},
                {"highlighted", MessageFlag::Highlighted},
                {"historic", MessageFlag::RecentMessage},
                {"firstmsg", MessageFlag::FirstMessage},
                {"restricted", MessageFlag::RestrictedMessage},
                {"hypechat", MessageFlag::ElevatedMessage},
                {"shared", MessageFlag::SharedMessage},
            };

            bool handled = false;
            for (const auto &mapping : FLAG_MAP)
            {
                if (opt.compare(QLatin1String(mapping.keyword),
                                Qt::CaseInsensitive) == 0)
                {
                    if (!message->flags.has(mapping.flag))
                    {
                        return false;
                    }
                    handled = true;
                    break;
                }
            }
            if (handled)
            {
                continue;
            }

            // config:any  — matches both info and regular messages (always true)
            if (opt.compare(QStringLiteral("any"), Qt::CaseInsensitive) == 0)
            {
                continue;
            }

            // config:shared|chan1|chan2  — shared message optionally from specific channel(s)
            if (opt.startsWith(QStringLiteral("shared|"), Qt::CaseInsensitive))
            {
                if (!message->flags.has(MessageFlag::SharedMessage))
                {
                    return false;
                }
                // Check that the message's channelName matches one of the listed channels
                const QStringList allowedChans =
                    opt.mid(7).split(QLatin1Char('|'), Qt::SkipEmptyParts);
                if (!allowedChans.isEmpty())
                {
                    bool chanMatch = false;
                    for (const QString &c : allowedChans)
                    {
                        if (message->channelName.compare(
                                c, Qt::CaseInsensitive) == 0)
                        {
                            chanMatch = true;
                            break;
                        }
                    }
                    if (!chanMatch)
                    {
                        return false;
                    }
                }
                continue;
            }

            // Unknown config option — treat as no-match
            return false;
        }
        return true;
    }

    // (plain)  — case-insensitive substring
    return value.contains(matcher, Qt::CaseInsensitive);
}

/// Returns true if @a matcher begins with a context-only prefix
/// (user:, chan:, status:, mystatus:, config:) that does not operate on a
/// text value.
bool isContextMatcher(const QString &matcher)
{
    return matcher.startsWith(QStringLiteral("user:"), Qt::CaseInsensitive) ||
           matcher.startsWith(QStringLiteral("chan:"), Qt::CaseInsensitive) ||
           matcher.startsWith(QStringLiteral("status:"), Qt::CaseInsensitive) ||
           matcher.startsWith(QStringLiteral("mystatus:"), Qt::CaseInsensitive) ||
           matcher.startsWith(QStringLiteral("config:"), Qt::CaseInsensitive);
}

// ─────────────────────────────────────────────
// Function dispatch
// ─────────────────────────────────────────────

/// Evaluate a single named function call.
/// Returns std::nullopt for unknown function names (so the call-site can
/// preserve the original text).
std::optional<QString> callFunction(const QString &funcName,
                                    const QStringList &args,
                                    const ChannelPtr &channel,
                                    const Message *message)
{
    // ── Text manipulation ────────────────────────────────────────────────────

    // $replace(text, needle, replacement)
    if (funcName == QStringLiteral("replace"))
    {
        if (args.size() < 3)
        {
            return args.value(0);
        }
        QString result = args[0];
        result.replace(args[1], args[2], Qt::CaseInsensitive);
        return result;
    }

    // $replaceRegex(text, pattern, replacement)
    if (funcName == QStringLiteral("replaceRegex"))
    {
        if (args.size() < 3)
        {
            return args.value(0);
        }
        const QRegularExpression rx(args[1]);
        if (!rx.isValid())
        {
            return args[0];
        }
        QString replaced = args[0];
        return replaced.replace(rx, args[2]);
    }

    // $join(sep, part1, part2, ...)
    if (funcName == QStringLiteral("join"))
    {
        if (args.size() < 1)
        {
            return QString();
        }
        return args.mid(1).join(args[0]);
    }

    // $lower(text)
    if (funcName == QStringLiteral("lower"))
    {
        return args.value(0).toLower();
    }

    // $upper(text)
    if (funcName == QStringLiteral("upper"))
    {
        return args.value(0).toUpper();
    }

    // $trim(text)
    if (funcName == QStringLiteral("trim"))
    {
        return args.value(0).trimmed();
    }

    // $len(text)
    if (funcName == QStringLiteral("len"))
    {
        return QString::number(args.value(0).length());
    }

    // $sub(text, start[, length])  —  0-based start
    if (funcName == QStringLiteral("sub"))
    {
        if (args.size() < 2)
        {
            return args.value(0);
        }
        bool ok{};
        int start = args[1].toInt(&ok);
        if (!ok)
        {
            return args[0];
        }
        if (start < 0)
        {
            start = static_cast<int>(std::max(qsizetype{0}, static_cast<qsizetype>(args[0].length()) + start));
        }
        if (args.size() >= 3)
        {
            bool ok2{};
            const int len = args[2].toInt(&ok2);
            return ok2 ? args[0].mid(start, len) : args[0].mid(start);
        }
        return args[0].mid(start);
    }

    // $word(text, n)  —  1-indexed
    if (funcName == QStringLiteral("word"))
    {
        if (args.size() < 2)
        {
            return QString();
        }
        const QStringList ws =
            args[0].split(QLatin1Char(' '), Qt::SkipEmptyParts);
        bool ok{};
        const int idx = args[1].toInt(&ok) - 1;
        if (!ok || idx < 0 || idx >= ws.size())
        {
            return QString();
        }
        return ws[idx];
    }

    // $words(text, from[, to])  —  1-indexed, inclusive
    if (funcName == QStringLiteral("words"))
    {
        if (args.size() < 2)
        {
            return args.value(0);
        }
        const QStringList ws =
            args[0].split(QLatin1Char(' '), Qt::SkipEmptyParts);
        bool ok{};
        const int from = args[1].toInt(&ok) - 1;
        if (!ok || from < 0 || from >= ws.size())
        {
            return QString();
        }
        int to = ws.size() - 1;
        if (args.size() >= 3)
        {
            bool ok2{};
            const int toArg = args[2].toInt(&ok2) - 1;
            if (ok2)
            {
                to = static_cast<int>(std::min(static_cast<qsizetype>(toArg), ws.size() - 1));
            }
        }
        if (from > to)
        {
            return QString();
        }
        return ws.mid(from, to - from + 1).join(QLatin1Char(' '));
    }

    // $charAt(text, n)  —  0-indexed
    if (funcName == QStringLiteral("charAt"))
    {
        if (args.size() < 2)
        {
            return QString();
        }
        bool ok{};
        const int idx = args[1].toInt(&ok);
        if (!ok || idx < 0 || idx >= args[0].length())
        {
            return QString();
        }
        return QString(args[0][idx]);
    }

    // ── Boolean predicates ───────────────────────────────────────────────────

    // $contains(haystack, needle)
    if (funcName == QStringLiteral("contains"))
    {
        if (args.size() < 2)
        {
            return QStringLiteral("false");
        }
        return args[0].contains(args[1], Qt::CaseInsensitive)
                   ? QStringLiteral("true")
                   : QStringLiteral("false");
    }

    // $startsWith(text, prefix)
    if (funcName == QStringLiteral("startsWith"))
    {
        if (args.size() < 2)
        {
            return QStringLiteral("false");
        }
        return args[0].startsWith(args[1], Qt::CaseInsensitive)
                   ? QStringLiteral("true")
                   : QStringLiteral("false");
    }

    // $endsWith(text, suffix)
    if (funcName == QStringLiteral("endsWith"))
    {
        if (args.size() < 2)
        {
            return QStringLiteral("false");
        }
        return args[0].endsWith(args[1], Qt::CaseInsensitive)
                   ? QStringLiteral("true")
                   : QStringLiteral("false");
    }

    // $eq(a, b)  —  case-insensitive equality
    if (funcName == QStringLiteral("eq"))
    {
        if (args.size() < 2)
        {
            return QStringLiteral("false");
        }
        return (args[0].compare(args[1], Qt::CaseInsensitive) == 0)
                   ? QStringLiteral("true")
                   : QStringLiteral("false");
    }

    // $neq(a, b)
    if (funcName == QStringLiteral("neq"))
    {
        if (args.size() < 2)
        {
            return QStringLiteral("true");
        }
        return (args[0].compare(args[1], Qt::CaseInsensitive) != 0)
                   ? QStringLiteral("true")
                   : QStringLiteral("false");
    }

    // $gt(a, b)  $lt(a, b)  $gte(a, b)  $lte(a, b)  —  numeric
    if (funcName == QStringLiteral("gt") || funcName == QStringLiteral("lt") ||
        funcName == QStringLiteral("gte") || funcName == QStringLiteral("lte"))
    {
        if (args.size() < 2)
        {
            return QStringLiteral("false");
        }
        bool ok1{};
        bool ok2{};
        const double a = args[0].toDouble(&ok1);
        const double b = args[1].toDouble(&ok2);
        if (!ok1 || !ok2)
        {
            return QStringLiteral("false");
        }
        bool result = false;
        if (funcName == QStringLiteral("gt"))
        {
            result = a > b;
        }
        else if (funcName == QStringLiteral("lt"))
        {
            result = a < b;
        }
        else if (funcName == QStringLiteral("gte"))
        {
            result = a >= b;
        }
        else
        {
            result = a <= b;  // lte
        }
        return result ? QStringLiteral("true") : QStringLiteral("false");
    }

    // ── Boolean logic ────────────────────────────────────────────────────────

    // $not(value)
    if (funcName == QStringLiteral("not"))
    {
        return (args.value(0) == QStringLiteral("true"))
                   ? QStringLiteral("false")
                   : QStringLiteral("true");
    }

    // $and(a, b, ...)
    if (funcName == QStringLiteral("and"))
    {
        for (const QString &arg : args)
        {
            if (arg != QStringLiteral("true"))
            {
                return QStringLiteral("false");
            }
        }
        return QStringLiteral("true");
    }

    // $or(a, b, ...)
    if (funcName == QStringLiteral("or"))
    {
        for (const QString &arg : args)
        {
            if (arg == QStringLiteral("true"))
            {
                return QStringLiteral("true");
            }
        }
        return QStringLiteral("false");
    }

    // ── Control flow ─────────────────────────────────────────────────────────

    // $if(condition, trueValue[, falseValue])
    if (funcName == QStringLiteral("if"))
    {
        if (args.size() < 2)
        {
            return QString();
        }
        const bool cond = (args[0] == QStringLiteral("true"));
        if (cond)
        {
            return args[1];
        }
        return args.value(2);  // empty QString if not provided
    }

    // ── Context / highlight matching ─────────────────────────────────────────

    // $is([value,] matcher...)
    if (funcName == QStringLiteral("is"))
    {
        if (args.empty())
        {
            return QStringLiteral("false");
        }

        // If the first arg begins with a context-matcher prefix (user:, chan:,
        // status:), treat all args as matchers with no separate value.
        // Otherwise the first arg is the value string.
        QString testValue;
        QStringList matchers;

        if (isContextMatcher(args[0]))
        {
            matchers = args;  // no value arg
        }
        else
        {
            testValue = args[0];
            matchers = args.mid(1);
        }

        for (const QString &matcher : matchers)
        {
            if (!evaluateMatcher(testValue, matcher, channel, message))
            {
                return QStringLiteral("false");
            }
        }
        return QStringLiteral("true");
    }

    return std::nullopt;  // Unknown function
}

// ─────────────────────────────────────────────
// Recursive evaluator (single pass, left to right)
// ─────────────────────────────────────────────

/// Internal recursive worker.  Called by evaluateFunctions() and by itself
/// when processing function arguments.
QString evalFunctionsInternal(const QString &input, const ChannelPtr &channel,
                              const Message *message)
{
    QString result;
    result.reserve(input.size());

    int i = 0;
    const int n = input.length();

    while (i < n)
    {
        // \$ → literal '$'
        if (i + 1 < n && input[i] == QLatin1Char('\\') &&
            input[i + 1] == QLatin1Char('$'))
        {
            result += QLatin1Char('$');
            i += 2;
            continue;
        }

        // Possible start of a function call
        if (input[i] == QLatin1Char('$'))
        {
            int j = i + 1;

            // First character of the function name must be a letter
            if (j < n && input[j].isLetter())
            {
                ++j;
                // Subsequent characters may be letters or digits
                while (j < n && input[j].isLetterOrNumber())
                {
                    ++j;
                }

                // Function name must be followed by '('
                if (j < n && input[j] == QLatin1Char('('))
                {
                    const QString funcName = input.mid(i + 1, j - i - 1);
                    const int argsStart = j + 1;

                    // Find the matching closing ')' respecting nesting
                    int depth = 1;
                    int scanPos = argsStart;
                    while (scanPos < n && depth > 0)
                    {
                        if (input[scanPos] == QLatin1Char('('))
                        {
                            ++depth;
                        }
                        else if (input[scanPos] == QLatin1Char(')'))
                        {
                            --depth;
                        }
                        ++scanPos;
                    }

                    if (depth == 0)
                    {
                        // scanPos is now one past the closing ')'
                        const int closePos = scanPos - 1;
                        const QString rawArgStr =
                            input.mid(argsStart, closePos - argsStart);

                        // Recursively evaluate functions inside arguments
                        const QString evalledArgStr =
                            evalFunctionsInternal(rawArgStr, channel, message);

                        // Split evaluated arg string into individual arguments
                        const QStringList args = splitArgs(evalledArgStr);

                        auto maybeResult =
                            callFunction(funcName, args, channel, message);

                        if (maybeResult.has_value())
                        {
                            result += *maybeResult;
                        }
                        else
                        {
                            // Unknown function — emit literally
                            result += QLatin1Char('$');
                            result += funcName;
                            result += QLatin1Char('(');
                            result += evalledArgStr;
                            result += QLatin1Char(')');
                        }

                        i = scanPos;  // advance past the closing ')'
                        continue;
                    }
                    // else: unmatched '(' — fall through to emit '$' literally
                }
            }
        }

        result += input[i];
        ++i;
    }

    return result;
}

}  // anonymous namespace

// ─────────────────────────────────────────────
// Public API
// ─────────────────────────────────────────────

QString evaluateFunctions(const QString &input, const ChannelPtr &channel,
                          const Message *message, const QStringList &words)
{
    (void)words;  // reserved for future $arg() style functions
    return evalFunctionsInternal(input, channel, message);
}

}  // namespace commands
}  // namespace chatterino
