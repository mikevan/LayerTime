package com.projectrevivesolutions.layertime.controller.flow

/**
 * A small JSON reader and writer for workflow files (2026-11-04). Dependency-free
 * on purpose: the app's main code carries no JSON library, Android's org.json is a
 * stub under host unit tests, and the flow package must test with plain kotlinc.
 * Objects become Map<String, Any?> (insertion order kept), arrays List<Any?>,
 * numbers Long or Double, plus String, Boolean, and null.
 */
object FlowJson {
    private val numberPattern = Regex("-?(0|[1-9][0-9]*)(\\.[0-9]+)?([eE][+-]?[0-9]+)?")
    fun parse(text: String): Any? {
        val p = Parser(text)
        p.skipWs()
        val v = p.value()
        p.skipWs()
        if (!p.atEnd()) throw FlowJsonError("unexpected text at ${p.pos}")
        return v
    }

    fun write(value: Any?): String = StringBuilder().also { write(value, it) }.toString()

    private fun write(v: Any?, out: StringBuilder) {
        when (v) {
            null -> out.append("null")
            is String -> quote(v, out)
            is Boolean -> out.append(v)
            is Int, is Long -> out.append(v)
            is Number -> out.append(v.toDouble())
            is Map<*, *> -> {
                out.append('{')
                var first = true
                for ((k, x) in v) {
                    if (!first) out.append(','); first = false
                    quote(k.toString(), out); out.append(':'); write(x, out)
                }
                out.append('}')
            }
            is List<*> -> {
                out.append('[')
                v.forEachIndexed { i, x -> if (i > 0) out.append(','); write(x, out) }
                out.append(']')
            }
            else -> quote(v.toString(), out)
        }
    }

    private fun quote(s: String, out: StringBuilder) {
        out.append('"')
        for (c in s) when (c) {
            '"' -> out.append("\\\""); '\\' -> out.append("\\\\"); '\n' -> out.append("\\n")
            '\r' -> out.append("\\r"); '\t' -> out.append("\\t")
            else -> if (c < ' ') out.append("\\u%04x".format(c.code)) else out.append(c)
        }
        out.append('"')
    }

    private class Parser(val s: String) {
        var pos = 0
        private var depth = 0
        private fun <T> nested(read: () -> T): T {
            if (depth >= 64) throw FlowJsonError("JSON nesting exceeds 64 levels")
            depth++
            return try { read() } finally { depth-- }
        }
        fun atEnd() = pos >= s.length
        fun skipWs() { while (pos < s.length && s[pos].isWhitespace()) pos++ }
        fun value(): Any? {
            skipWs()
            if (atEnd()) throw FlowJsonError("unexpected end of text")
            return when (s[pos]) {
                '{' -> nested { obj() }; '[' -> nested { arr() }; '"' -> str()
                't' -> word("true", true); 'f' -> word("false", false); 'n' -> word("null", null)
                else -> num()
            }
        }
        fun obj(): Map<String, Any?> {
            val m = LinkedHashMap<String, Any?>()
            pos++; skipWs()
            if (peek() == '}') { pos++; return m }
            while (true) {
                skipWs()
                if (peek() != '"') throw FlowJsonError("expected a key at $pos")
                val k = str(); skipWs()
                if (m.containsKey(k)) throw FlowJsonError("duplicate object key")
                if (peek() != ':') throw FlowJsonError("expected ':' at $pos"); pos++
                m[k] = value(); skipWs()
                when (peek()) { ',' -> pos++; '}' -> { pos++; return m }; else -> throw FlowJsonError("expected ',' or '}' at $pos") }
            }
        }
        fun arr(): List<Any?> {
            val l = ArrayList<Any?>()
            pos++; skipWs()
            if (peek() == ']') { pos++; return l }
            while (true) {
                l.add(value()); skipWs()
                when (peek()) { ',' -> pos++; ']' -> { pos++; return l }; else -> throw FlowJsonError("expected ',' or ']' at $pos") }
            }
        }
        fun str(): String {
            pos++
            val b = StringBuilder()
            while (true) {
                if (atEnd()) throw FlowJsonError("unterminated string")
                val c = s[pos++]
                when (c) {
                    '"' -> return b.toString()
                    '\\' -> {
                        if (atEnd()) throw FlowJsonError("unterminated escape")
                        when (val e = s[pos++]) {
                            '"' -> b.append('"'); '\\' -> b.append('\\'); '/' -> b.append('/')
                            'b' -> b.append('\b'); 'f' -> b.append('\u000C'); 'n' -> b.append('\n')
                            'r' -> b.append('\r'); 't' -> b.append('\t')
                            'u' -> { b.append(s.substring(pos, pos + 4).toInt(16).toChar()); pos += 4 }
                            else -> throw FlowJsonError("bad escape \\$e at ${pos - 1}")
                        }
                    }
                    else -> { if(c < ' ') throw FlowJsonError("unescaped control character"); b.append(c) }
                }
            }
        }
        fun num(): Any {
            val start = pos
            while (!atEnd() && (s[pos].isDigit() || s[pos] in "+-.eE")) pos++
            val t = s.substring(start, pos)
            if (t.isEmpty()) throw FlowJsonError("unexpected '${s[pos]}' at $pos")
            if (!t.matches(numberPattern)) throw FlowJsonError("invalid JSON number")
            return if (t.any { it in ".eE" }) (t.toDoubleOrNull()?.takeIf {it.isFinite()} ?: throw FlowJsonError("number out of range"))
                else t.toLongOrNull() ?: throw FlowJsonError("integer out of range")
        }
        fun word(w: String, v: Any?): Any? {
            if (!s.startsWith(w, pos)) throw FlowJsonError("unexpected text at $pos"); pos += w.length; return v
        }
        fun peek(): Char = if (atEnd()) throw FlowJsonError("unexpected end of text") else s[pos]
    }
}

class FlowJsonError(message: String) : Exception(message)
