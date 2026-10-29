package com.projectrevivesolutions.layertime.controller.tools

/**
 * Minimal, dependency-free JSON reader. Just enough to read
 * contracts/vectors/link_frames.json so the Link codec can be checked against
 * the canonical vectors on a host JVM with no third-party libraries. Numbers
 * are returned as Long when integral, Double otherwise.
 *
 * This is a test utility, not a general-purpose parser.
 */
object MiniJson {
    fun parse(text: String): Any? = Parser(text).run {
        val v = value()
        skipWs()
        check(atEnd()) { "trailing content at $pos" }
        v
    }

    private class Parser(val s: String) {
        var pos = 0
        fun atEnd() = pos >= s.length
        fun skipWs() { while (pos < s.length && s[pos].isWhitespace()) pos++ }

        fun value(): Any? {
            skipWs()
            return when (val c = s[pos]) {
                '{' -> obj()
                '[' -> arr()
                '"' -> str()
                't', 'f' -> bool()
                'n' -> { expect("null"); null }
                else -> if (c == '-' || c.isDigit()) num() else error("unexpected '$c' at $pos")
            }
        }

        fun obj(): Map<String, Any?> {
            val m = LinkedHashMap<String, Any?>()
            pos++ // {
            skipWs()
            if (s[pos] == '}') { pos++; return m }
            while (true) {
                skipWs()
                val k = str()
                skipWs(); expectChar(':')
                m[k] = value()
                skipWs()
                when (s[pos]) { ',' -> { pos++ }; '}' -> { pos++; return m }; else -> error("expected , or } at $pos") }
            }
        }

        fun arr(): List<Any?> {
            val l = ArrayList<Any?>()
            pos++ // [
            skipWs()
            if (s[pos] == ']') { pos++; return l }
            while (true) {
                l.add(value())
                skipWs()
                when (s[pos]) { ',' -> { pos++ }; ']' -> { pos++; return l }; else -> error("expected , or ] at $pos") }
            }
        }

        fun str(): String {
            expectChar('"')
            val sb = StringBuilder()
            while (true) {
                val c = s[pos++]
                when (c) {
                    '"' -> return sb.toString()
                    '\\' -> {
                        val e = s[pos++]
                        when (e) {
                            '"' -> sb.append('"'); '\\' -> sb.append('\\'); '/' -> sb.append('/')
                            'n' -> sb.append('\n'); 't' -> sb.append('\t'); 'r' -> sb.append('\r')
                            'b' -> sb.append('\b'); 'f' -> sb.append('\u000C')
                            'u' -> { val h = s.substring(pos, pos + 4); pos += 4; sb.append(h.toInt(16).toChar()) }
                            else -> error("bad escape \\$e at $pos")
                        }
                    }
                    else -> sb.append(c)
                }
            }
        }

        fun num(): Any {
            val start = pos
            if (s[pos] == '-') pos++
            while (pos < s.length && (s[pos].isDigit() || s[pos] in ".eE+-")) pos++
            val t = s.substring(start, pos)
            return if (t.any { it == '.' || it == 'e' || it == 'E' }) t.toDouble() else t.toLong()
        }

        fun bool(): Boolean = if (s.startsWith("true", pos)) { pos += 4; true }
            else { expect("false"); false }

        fun expect(word: String) { check(s.startsWith(word, pos)) { "expected '$word' at $pos" }; pos += word.length }
        fun expectChar(c: Char) { check(s[pos] == c) { "expected '$c' at $pos" }; pos++ }
    }
}

/** Convenience accessors over the parsed tree. */
@Suppress("UNCHECKED_CAST")
fun Any?.obj(): Map<String, Any?> = this as Map<String, Any?>
@Suppress("UNCHECKED_CAST")
fun Any?.arr(): List<Any?> = this as List<Any?>
fun Map<String, Any?>.long(k: String): Long = (this[k] as Number).toLong()
fun Map<String, Any?>.int(k: String): Int = (this[k] as Number).toInt()
fun Map<String, Any?>.str(k: String): String = this[k] as String
fun Map<String, Any?>.has(k: String): Boolean = containsKey(k)
