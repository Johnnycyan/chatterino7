// SPDX-FileCopyrightText: 2024 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#pragma once

#include <QString>
#include <QStringList>

#include <memory>

namespace chatterino {

class Channel;
using ChannelPtr = std::shared_ptr<Channel>;
struct Message;

namespace commands {

/**
 * Evaluates Chatty-style command functions in @a input.
 *
 * Functions use the syntax:  $funcName(arg1, arg2, ...)
 * Nested calls are supported and evaluated innermost-first.
 * Use \$ to produce a literal '$' (escape).
 *
 * The @a words list and @a channel / @a message context are available to
 * context-sensitive functions such as $is().
 *
 * ── Text manipulation ──────────────────────────────────────────
 *   $replace(text, needle, replacement)
 *       Case-insensitive substring replacement (all occurrences).
 *   $replaceRegex(text, pattern, replacement)
 *       Regex-based replacement; capture groups \1..\9 work.
 *   $join(sep, part1, part2, ...)
 *       Join all parts after the first with sep.
 *   $lower(text)          → lower-case
 *   $upper(text)          → upper-case
 *   $trim(text)           → strip leading & trailing whitespace
 *   $len(text)            → character count as decimal string
 *   $sub(text, start[, length])   → substring (0-based start)
 *   $word(text, n)        → nth word (1-indexed, space-split)
 *   $words(text, from[, to])      → word range (1-indexed, inclusive)
 *   $charAt(text, n)      → single character at 0-based index
 *
 * ── Boolean predicates (return "true" / "false") ───────────────
 *   $contains(haystack, needle)   case-insensitive substring check
 *   $startsWith(text, prefix)
 *   $endsWith(text, suffix)
 *   $eq(a, b)   $neq(a, b)       case-insensitive string equality
 *   $gt(a,b)  $lt(a,b)  $gte(a,b)  $lte(a,b)   numeric comparison
 *   $not(value)           invert "true"/"false"
 *   $and(a, b, ...)       "true" only if every arg is "true"
 *   $or(a, b, ...)        "true" if any arg is "true"
 *
 * ── Control flow ───────────────────────────────────────────────
 *   $if(condition, trueValue[, falseValue])
 *       Returns trueValue when condition == "true", else falseValue
 *       (or empty string if falseValue is omitted).
 *
 * ── Context / highlight matching ($is) ─────────────────────────
 *   $is([value,] matcher...)
 *       All matchers must be satisfied (AND semantics).
 *
 *       Matcher types:
 *         user:name       message's login name equals name (case-insensitive)
 *         chan:channel    current channel name equals channel (case-insensitive)
 *         status:flags    message author has the given status badges:
 *                           b = broadcaster   m = moderator   s = subscriber
 *                           v = VIP           t = turbo       p = prime/premium
 *         mystatus:flags  the LOCAL USER has the given role in the channel:
 *                           b = broadcaster   m = moderator   v = VIP
 *         config:option[|param][,option...]  message/channel state flags:
 *                           info         system/info message (not a user chat msg)
 *                           any          both info and regular messages
 *                           hl           channel-point-redeemed highlight
 *                           highlighted  matched by highlight list
 *                           historic     loaded from history service
 *                           firstmsg     user's first message in channel
 *                           restricted   low-trust restricted user message
 *                           hypechat     elevated (hype-chat) message
 *                           shared[|c1|c2]  shared-chat message, optionally from channel(s)
 *                           live[|regex] stream is live; regex matched against title/game
 *                           !live        stream is not live
 *                           b|key[/ver]  message has Twitch badge key (version optional)
 *         re:pattern      value matches the regex pattern
 *         cs:text         value contains text (case-sensitive)
 *         w:text          value contains text as a whole word (case-insensitive)
 *         start:text      value starts with text (case-insensitive)
 *         end:text        value ends with text (case-insensitive)
 *         (plain text)    value contains text (case-insensitive)
 *
 *       If the first argument begins with a context-matcher prefix
 *       (user:, chan:, status:), no value argument is expected and all
 *       arguments are treated as matchers.
 *       Otherwise the first argument is the value and the remaining
 *       arguments are matchers.
 *
 *   Examples:
 *     $is(user:badguy)               → true when message is from "badguy"
 *     $is(chan:xqc, status:s)        → true in xqc's channel for subscribers
 *     $is({msg.text}, w:hello)       → true if message contains whole word "hello"
 *     $if($is(status:m), [mod], )    → "[mod]" for mods, empty otherwise
 */
QString evaluateFunctions(const QString &input, const ChannelPtr &channel,
                          const Message *message, const QStringList &words);

}  // namespace commands
}  // namespace chatterino
