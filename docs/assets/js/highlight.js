/**
 * Nevaarize Syntax Highlighter
 * Lightweight syntax highlighting for Nevaarize code blocks
 */

(function() {
    'use strict';

    // Nevaarize language keywords
    const KEYWORDS = [
        'function', 'async', 'await', 'return', 'if', 'elif', 'else',
        'for', 'while', 'in', 'break', 'continue',
        'struct', 'import', 'as', 'stdlib',
        'true', 'false', 'nil', 'and', 'or', 'not'
    ];

    // Built-in functions
    const BUILTINS = [
        'print', 'len', 'type', 'str', 'int', 'float', 'Range',
        'push', 'pop', 'append',
        'nativeSumLoop', 'nativeFibLoop', 'nativeCallLoop',
        'jitSumLoop', 'simdInfo', 'simdSumLoop', 'simdDotProduct',
        'matmulBenchmark', 'reluBenchmark'
    ];

    /**
     * Escape HTML special characters
     */
    function escapeHtml(text) {
        const map = {
            '&': '&amp;',
            '<': '&lt;',
            '>': '&gt;',
            '"': '&quot;',
            "'": '&#039;'
        };
        return text.replace(/[&<>"']/g, char => map[char]);
    }

    /**
     * Tokenize Nevaarize code
     */
    function tokenize(code) {
        const tokens = [];
        let i = 0;

        while (i < code.length) {
            // Skip whitespace but preserve it
            if (/\s/.test(code[i])) {
                let ws = '';
                while (i < code.length && /\s/.test(code[i])) {
                    ws += code[i];
                    i++;
                }
                tokens.push({ type: 'whitespace', value: ws });
                continue;
            }

            // Comments
            if (code[i] === '/' && code[i + 1] === '/') {
                let comment = '';
                while (i < code.length && code[i] !== '\n') {
                    comment += code[i];
                    i++;
                }
                tokens.push({ type: 'comment', value: comment });
                continue;
            }

            // Strings (double quotes)
            if (code[i] === '"') {
                let str = '"';
                i++;
                while (i < code.length && code[i] !== '"') {
                    if (code[i] === '\\' && i + 1 < code.length) {
                        str += code[i] + code[i + 1];
                        i += 2;
                    } else {
                        str += code[i];
                        i++;
                    }
                }
                if (i < code.length) {
                    str += '"';
                    i++;
                }
                tokens.push({ type: 'string', value: str });
                continue;
            }

            // Numbers
            if (/[0-9]/.test(code[i]) || (code[i] === '.' && /[0-9]/.test(code[i + 1]))) {
                let num = '';
                while (i < code.length && /[0-9.]/.test(code[i])) {
                    num += code[i];
                    i++;
                }
                tokens.push({ type: 'number', value: num });
                continue;
            }

            // Identifiers and keywords
            if (/[a-zA-Z_]/.test(code[i])) {
                let ident = '';
                while (i < code.length && /[a-zA-Z0-9_]/.test(code[i])) {
                    ident += code[i];
                    i++;
                }
                
                if (KEYWORDS.includes(ident)) {
                    tokens.push({ type: 'keyword', value: ident });
                } else if (BUILTINS.includes(ident)) {
                    tokens.push({ type: 'builtin', value: ident });
                } else {
                    tokens.push({ type: 'identifier', value: ident });
                }
                continue;
            }

            // Operators
            if (/[+\-*/%=<>!&|]/.test(code[i])) {
                let op = code[i];
                i++;
                // Check for multi-character operators
                if (i < code.length && /[=<>&|]/.test(code[i])) {
                    op += code[i];
                    i++;
                }
                tokens.push({ type: 'operator', value: op });
                continue;
            }

            // Punctuation
            if (/[(){}[\],.;:]/.test(code[i])) {
                tokens.push({ type: 'punctuation', value: code[i] });
                i++;
                continue;
            }

            // Other characters
            tokens.push({ type: 'other', value: code[i] });
            i++;
        }

        return tokens;
    }

    /**
     * Highlight tokens and generate HTML
     */
    function highlightTokens(tokens) {
        return tokens.map(token => {
            const escaped = escapeHtml(token.value);
            
            switch (token.type) {
                case 'keyword':
                    return `<span class="token-keyword">${escaped}</span>`;
                case 'builtin':
                    return `<span class="token-function">${escaped}</span>`;
                case 'string':
                    return `<span class="token-string">${escaped}</span>`;
                case 'number':
                    return `<span class="token-number">${escaped}</span>`;
                case 'comment':
                    return `<span class="token-comment">${escaped}</span>`;
                case 'operator':
                    return `<span class="token-operator">${escaped}</span>`;
                case 'punctuation':
                    return `<span class="token-punctuation">${escaped}</span>`;
                default:
                    return escaped;
            }
        }).join('');
    }

    /**
     * Highlight a code block
     */
    function highlightCodeBlock(codeElement) {
        const code = codeElement.textContent;
        const tokens = tokenize(code);
        codeElement.innerHTML = highlightTokens(tokens);
    }

    /**
     * Highlight all Nevaarize code blocks on the page
     */
    function highlightAll() {
        // Find all pre blocks with data-lang="nevaarize"
        const preBlocks = document.querySelectorAll('pre[data-lang="nevaarize"]');
        
        preBlocks.forEach(pre => {
            const codeElement = pre.querySelector('code');
            if (codeElement) {
                highlightCodeBlock(codeElement);
            }
        });
    }

    // Run when DOM is ready
    if (document.readyState === 'loading') {
        document.addEventListener('DOMContentLoaded', highlightAll);
    } else {
        highlightAll();
    }

    // Expose API for manual highlighting
    window.NevaarizeHighlight = {
        highlight: highlightCodeBlock,
        highlightAll: highlightAll,
        tokenize: tokenize
    };

})();
