const sampleSelect = document.getElementById("sampleSelect");
const loadSampleButton = document.getElementById("loadSample");
const runSampleButton = document.getElementById("runSample");
const runCustomButton = document.getElementById("runCustom");
const resetEditorButton = document.getElementById("resetEditor");
const editor = document.getElementById("editor");
const samplePreview = document.getElementById("samplePreview");
const statusBar = document.getElementById("statusBar");
const traceView = document.getElementById("traceView");
const rawView = document.getElementById("rawView");
const finalView = document.getElementById("finalView");

const grammarText = document.getElementById("grammarText");
const firstFollowText = document.getElementById("firstFollowText");
const ll1Text = document.getElementById("ll1Text");
const lrText = document.getElementById("lrText");
const integrationText = document.getElementById("integrationText");


const tabs = Array.from(document.querySelectorAll(".tab"));
const docTabs = Array.from(document.querySelectorAll(".docs-tab"));
const docViews = Array.from(document.querySelectorAll(".docs-tabview"));

let currentSampleSource = "";

function setStatus(text, mode, append = false) {
  if (append) {
    const el = document.createElement("div");
    el.className = `status-item ${mode}`;
    el.textContent = "• " + text;
    statusBar.appendChild(el);
    statusBar.scrollTop = statusBar.scrollHeight;
  } else {
    statusBar.className = `status ${mode}`;
    statusBar.innerHTML = `<div class="status-item ${mode}">• ${escapeHtml(text)}</div>`;
  }
}

function escapeHtml(text) {
  return text
    .replace(/&/g, "&amp;")
    .replace(/</g, "&lt;")
    .replace(/>/g, "&gt;")
    .replace(/"/g, "&quot;")
    .replace(/'/g, "&#39;");
}

function splitTrace(text) {
  const lines = text.split(/\r?\n/);
  const sections = [];
  let currentTitle = null;
  let current = [];

  for (let i = 0; i < lines.length; i += 1) {
    const line = lines[i].trim();
    const marker = line === "========================================";
    const hasTitle = i + 2 < lines.length && lines[i + 2].trim() === "========================================";
    if (marker && hasTitle) {
      if (currentTitle !== null) {
        sections.push({ title: currentTitle, body: current.join("\n").trim() });
        current = [];
      }
      currentTitle = lines[i + 1].trim();
      i += 2;
      continue;
    }
    if (currentTitle !== null) {
      current.push(lines[i]);
    }
  }

  if (currentTitle !== null) {
    sections.push({ title: currentTitle, body: current.join("\n").trim() });
  }

  return sections;
}

function splitColumns(line) {
  return line.trim().split(/\s{2,}/).map((part) => part.trim()).filter(Boolean);
}

function parseTableBody(body) {
  const lines = body.split(/\r?\n/).filter((line) => line.trim().length > 0);
  if (lines.length < 3) {
    return null;
  }

  const header = lines[0].trim();
  const separator = lines[1].trim();
  if (!/^[-\s]+$/.test(separator)) {
    return null;
  }

  const headers = splitColumns(header);
  if (!headers.length) {
    return null;
  }

  const rows = [];
  for (const line of lines.slice(2)) {
    const columns = splitColumns(line);
    if (!columns.length) {
      break;
    }
    if (columns.length < headers.length) {
      break;
    }
    rows.push(columns);
  }

  if (!rows.length) {
    return null;
  }

  return { headers, rows };
}

function parseKnownTable(body, headers) {
  const lines = body.split(/\r?\n/).filter((line) => line.trim().length > 0);
  if (lines.length < 3) {
    return null;
  }

  const rows = [];
  for (const line of lines.slice(2)) {
    const columns = splitColumns(line);
    if (!columns.length) {
      break;
    }
    if (columns.length < headers.length) {
      break;
    }
    rows.push(columns.slice(0, headers.length));
  }

  if (!rows.length) {
    return null;
  }

  return { headers, rows };
}

function parseFirstFollowTable(body) {
  const rows = [];
  for (const rawLine of body.split(/\r?\n/)) {
    const line = rawLine.trim();
    if (!line || line.startsWith("FIRST/FOLLOW SETS")) {
      continue;
    }
    const match = line.match(/^(\S+)\s+FIRST=\{(.*?)\}\s+FOLLOW=\{(.*?)\}$/);
    if (match) {
      rows.push([match[1], match[2], match[3]]);
    }
  }
  return rows.length ? { headers: ["Nonterminal", "FIRST", "FOLLOW"], rows } : null;
}

function parseLl1Table(body) {
  const rows = [];
  for (const rawLine of body.split(/\r?\n/)) {
    const line = rawLine.trim();
    if (!line || line.startsWith("LL(1) Parsing Table Entries:")) {
      continue;
    }
    const match = line.match(/^\[(.+?),\s*(.+?)\]\s*->\s*prod\s*(\d+)\s*:\s*(.+)$/);
    if (match) {
      rows.push([match[1], match[2], match[3], match[4]]);
    }
  }
  return rows.length ? { headers: ["Nonterminal", "Terminal", "Prod", "Rule"], rows } : null;
}

function parseDocLrTable(body) {
  const rows = [];
  for (const rawLine of body.split(/\r?\n/)) {
    const line = rawLine.trim();
    if (!line || line.startsWith("LR ACTION entries:")) {
      continue;
    }
    const match = line.match(/^state\s+(\d+),\s+token\s+'([^']+)'\s*->\s*(.+)$/i);
    if (match) {
      rows.push([match[1], match[2], match[3]]);
    }
  }
  return rows.length ? { headers: ["State", "Token", "Action"], rows } : null;
}

function parseGrammarTable(body) {
  const rows = [];
  for (const rawLine of body.split(/\r?\n/)) {
    const line = rawLine.trim();
    if (!line || line.startsWith("#") || line.startsWith("This is") || line.startsWith("```")) {
      continue;
    }
    const match = line.match(/^([A-Za-z_][A-Za-z0-9_]*)\s*->\s*(.+)$/);
    if (match) {
      rows.push([match[1], match[2]]);
    }
  }
  return rows.length ? { headers: ["Nonterminal", "Production"], rows } : null;
}

function parseLrTable(body) {
  const lines = body.split(/\r?\n/);
  const rows = [];

  for (const rawLine of lines) {
    const line = rawLine.trimEnd();
    const trimmed = line.trim();
    if (!trimmed) {
      continue;
    }
    if (/^(LR PARSER RESULT|No errors reported|PROJECT COMBINATION STATUS)/i.test(trimmed)) {
      break;
    }
    if (/^Step\s+States\s+Input\s+Action/i.test(trimmed)) {
      continue;
    }
    if (/^[-]+$/.test(trimmed)) {
      continue;
    }

    const match = line.match(/^\s*(\d+)\s+([0-9 ]+?)\s{2,}(.+?)\s{2,}(.+)$/);
    if (match) {
      rows.push([match[1], match[2].trim(), match[3].trim(), match[4].trim()]);
      continue;
    }

    if (rows.length) {
      break;
    }
  }

  if (!rows.length) {
    return null;
  }

  return { headers: ["Step", "States", "Input", "Action"], rows };
}

function parsePredictiveTable(body) {
  const lines = body.split(/\r?\n/);
  const rows = [];

  for (const rawLine of lines) {
    const line = rawLine.trimEnd();
    const trimmed = line.trim();
    if (!trimmed) {
      continue;
    }
    if (/^(PREDICTIVE RESULT|No errors reported|Error summary)/i.test(trimmed)) {
      break;
    }
    if (/^Step\s+Stack\s+Input\s+Action/i.test(trimmed)) {
      continue;
    }
    if (/^[-]+$/.test(trimmed)) {
      continue;
    }

    const columns = splitColumns(line);
    if (columns.length >= 4 && /^\d+$/.test(columns[0])) {
      const step = columns[0];
      const action = columns.slice(3).join("  ");
      rows.push([step, columns[1], columns[2], action]);
      continue;
    }

    if (rows.length) {
      break;
    }
  }

  return rows.length ? { headers: ["Step", "Stack", "Input", "Action"], rows } : null;
}

function parseTraceLog(body) {
  const lines = body.split(/\r?\n/);
  const rows = [];
  for (let i = 0; i < lines.length; i += 1) {
    const rawLine = lines[i];
    if (!rawLine.trim()) {
      continue;
    }

    const line = rawLine.replace(/\s+$/, "");
    const trimmed = line.trim();
    if (/^(RDP RESULT:|No errors reported\.|Error summary:)/i.test(trimmed)) {
      continue;
    }

    if (/^[+][\-+\s]+$/.test(trimmed) && i + 2 < lines.length && /^\|/.test(lines[i + 1].trim()) && /^[+][\-+\s]+$/.test(lines[i + 2].trim())) {
      const block = [line];
      i += 1;
      while (i < lines.length) {
        const blockLine = lines[i].replace(/\s+$/, "");
        block.push(blockLine);
        if (/^[+][\-+\s]+$/.test(blockLine.trim()) && block.length > 3) {
          break;
        }
        i += 1;
      }
      const table = parseTableBody(block.join("\n"));
      if (table) {
        rows.push({ depth: 0, type: "Table", message: "Symbol table dump", table });
        continue;
      }
    }

    let type = "Trace";
    let message = trimmed;
    if (/^->\s*/.test(trimmed)) {
      type = "Enter";
      message = trimmed.replace(/^->\s*/, "");
    } else if (/^<-\s*/.test(trimmed)) {
      type = "Exit";
      message = trimmed.replace(/^<-\s*/, "");
    } else if (/^match\(/.test(trimmed)) {
      type = "Match";
    } else if (/^\[Scope\s+\d+\]/i.test(trimmed)) {
      type = "Scope";
    } else if (/^ERROR\s+/i.test(trimmed) || /^\[Semantic\]/i.test(trimmed) || /^\[Syntax\]/i.test(trimmed)) {
      type = "Error";
    }

    const indent = line.match(/^\s*/)?.[0].length || 0;
    rows.push({ depth: Math.floor(indent / 2), type, message });
  }

  return rows.length ? { headers: ["Depth", "Type", "Message"], rows } : null;
}

function renderMiniTable(headers, rows) {
  return `
    <div class="table-wrap nested-table-wrap">
      <table class="trace-table nested-table">
        <thead>
          <tr>${headers.map((header) => `<th>${escapeHtml(header)}</th>`).join("")}</tr>
        </thead>
        <tbody>
          ${rows.map((row) => `<tr>${headers.map((_, index) => `<td>${escapeHtml(row[index] || "")}</td>`).join("")}</tr>`).join("")}
        </tbody>
      </table>
    </div>
  `;
}

function renderTraceLog(body) {
  const table = parseTraceLog(body);
  if (!table) {
    return `<pre class="trace-pre">${escapeHtml(body || "")}</pre>`;
  }

  return `
    <div class="table-wrap trace-log-wrap">
      <table class="trace-table trace-log-table">
        <thead>
          <tr>${table.headers.map((header) => `<th>${escapeHtml(header)}</th>`).join("")}</tr>
        </thead>
        <tbody>
          ${table.rows.map((row) => {
            const depth = Number(row.depth || 0);
            const typeClass = `kind-${String(row.type || "trace").toLowerCase()}`;
            return `
              <tr>
                <td class="depth-cell">${escapeHtml(String(depth))}</td>
                <td><span class="trace-badge ${typeClass}">${escapeHtml(row.type || "")}</span></td>
                <td class="trace-message" style="padding-left: calc(${depth} * 0.95rem + 0.25rem);">${row.table ? renderMiniTable(row.table.headers, row.table.rows) : escapeHtml(row.message || "")}</td>
              </tr>
            `;
          }).join("")}
        </tbody>
      </table>
    </div>
  `;
}

function renderTable(headers, rows) {
  return `
    <div class="table-wrap">
      <table class="trace-table">
        <thead>
          <tr>${headers.map((header) => `<th>${escapeHtml(header)}</th>`).join("")}</tr>
        </thead>
        <tbody>
          ${rows.map((row) => `<tr>${headers.map((_, index) => `<td>${escapeHtml(row[index] || "")}</td>`).join("")}</tr>`).join("")}
        </tbody>
      </table>
    </div>
  `;
}

function getSectionByTitle(sections, needle) {
  const target = needle.toUpperCase();
  return sections.find((section) => section.title.toUpperCase().includes(target));
}

function renderModuleBody(section, preferredView, moduleId) {
  if (!section) {
    return '<div class="trace-empty">No trace data found for this module.</div>';
  }

  const table = moduleId === "lr"
    ? parseLrTable(section.body)
    : moduleId === "lexer"
      ? parseKnownTable(section.body, ["Token", "Lexeme", "Line", "Column"])
      : moduleId === "predictive"
        ? parsePredictiveTable(section.body)
        : (moduleId === "rdp" || moduleId === "semantic")
          ? parseTraceLog(section.body)
      : parseTableBody(section.body);
  if (preferredView === "table" && table) {
    return renderTable(table.headers, table.rows);
  }
  if (moduleId === "rdp" || moduleId === "semantic") {
    return renderTraceLog(section.body || "");
  }

  return `<pre class="trace-pre">${escapeHtml(section.body || "")}</pre>`;
}

function extractErrorHandlerText(sections, output) {
  const lines = output.split(/\r?\n/);
  const collected = [];
  const seen = new Set();
  let sawError = false;

  for (const rawLine of lines) {
    const line = rawLine.trim();
    if (!line) {
      continue;
    }
    if (/^(\[Syntax\]|\[Semantic\]|ERROR\s+|Error summary:)/i.test(line)) {
      sawError = true;
      if (line === "Error summary:") {
        if (collected[collected.length - 1] !== line) {
          collected.push(line);
        }
        continue;
      }
      if (!seen.has(line)) {
        seen.add(line);
        collected.push(line);
      }
      continue;
    }
  }

  if (!sawError) {
    collected.push("No errors reported.");
    return collected.join("\n");
  }

  return collected.join("\n");
}

function parseErrorEntries(output) {
  const entries = [];
  const seen = new Set();

  for (const rawLine of output.split(/\r?\n/)) {
    const line = rawLine.trim();
    if (!line || line === "Error summary:") {
      continue;
    }

    let kind = null;
    let title = line;
    let detail = "";
    let location = "";

    const semanticMatch = line.match(/^\[Semantic\]\s+line\s+(\d+),\s+column\s+(\d+):\s+(.+)$/i);
    const syntaxMatch = line.match(/^\[Syntax\]\s+line\s+(\d+),\s+column\s+(\d+):\s+(.+)$/i);
    const errorMatch = line.match(/^ERROR\s+line\s+(\d+):\s+(.+)$/i);

    if (semanticMatch) {
      kind = "semantic";
      location = `line ${semanticMatch[1]}, column ${semanticMatch[2]}`;
      title = semanticMatch[3];
      detail = semanticMatch[3];
    } else if (syntaxMatch) {
      kind = "syntax";
      location = `line ${syntaxMatch[1]}, column ${syntaxMatch[2]}`;
      title = syntaxMatch[3];
      detail = syntaxMatch[3];
    } else if (errorMatch) {
      kind = "error";
      location = `line ${errorMatch[1]}`;
      title = errorMatch[2];
      detail = errorMatch[2];
    } else {
      continue;
    }

    const key = `${kind}|${location}|${title}`;
    if (seen.has(key)) {
      continue;
    }
    seen.add(key);
    entries.push({ kind, location, title, detail });
  }

  return entries;
}

function renderErrorHandlerPanel(output) {
  const entries = parseErrorEntries(output);
  if (!entries.length) {
    return `<pre class="trace-pre">No errors reported.</pre>`;
  }

  return `
    <div class="error-summary-shell">
      <div class="error-summary-head">
        <span class="error-summary-kicker">Error Handler</span>
        <span class="error-summary-count">${entries.length} issue${entries.length === 1 ? "" : "s"}</span>
      </div>
      <div class="error-card-list">
        ${entries.map((entry) => `
          <article class="error-card error-${entry.kind}">
            <div class="error-card-top">
              <span class="error-pill error-${entry.kind}">${escapeHtml(entry.kind)}</span>
              <span class="error-location">${escapeHtml(entry.location)}</span>
            </div>
            <div class="error-message">${escapeHtml(entry.title)}</div>
          </article>
        `).join("")}
      </div>
    </div>
  `;
}

function renderTrace(sections) {
  const traceModules = [
    { id: "lexer", label: "Lexer", title: "LEXICAL ANALYZER", view: "table" },
    { id: "rdp", label: "RDP", title: "RECURSIVE DESCENT PARSER", view: "log" },
    { id: "predictive", label: "Predictive", title: "NON-RECURSIVE PREDICTIVE PARSER", view: "table" },
    { id: "semantic", label: "Semantic", title: "SEMANTIC ANALYZER", view: "log" },
    { id: "errorhandler", label: "Error Handler", title: null, view: "pre" },
    { id: "lr", label: "LR Steps", title: "LR PARSER", view: "table" },
  ];

  const tabsHtml = traceModules.map((module, index) => `
    <button class="trace-tab${index === 0 ? " active" : ""}" data-trace-tab="${module.id}">${module.label}</button>
  `).join("");

  const panesHtml = traceModules.map((module, index) => {
    const section = module.title ? getSectionByTitle(sections, module.title) : null;
    const errorHandlerText = module.id === "errorhandler" ? extractErrorHandlerText(sections, sections.map((entry) => entry.body).join("\n")) : "";
    return `
      <div class="trace-pane${index === 0 ? " active" : ""}" id="trace-${module.id}">
        ${section ? `<div class="trace-pane-title">${escapeHtml(section.title)}</div>` : `<div class="trace-pane-title">ERROR HANDLER</div>`}
        ${module.id === "errorhandler" ? renderErrorHandlerPanel(errorHandlerText) : renderModuleBody(section, module.view, module.id)}
      </div>
    `;
  }).join("");

  traceView.innerHTML = `
    <div class="trace-shell">
      <div class="trace-tabbar">${tabsHtml}</div>
      ${panesHtml}
    </div>
  `;

  traceView.querySelectorAll(".trace-tab").forEach((tab) => {
    tab.addEventListener("click", () => {
      const target = tab.dataset.traceTab;
      traceView.querySelectorAll(".trace-tab").forEach((button) => button.classList.toggle("active", button === tab));
      traceView.querySelectorAll(".trace-pane").forEach((pane) => pane.classList.toggle("active", pane.id === `trace-${target}`));
    });
  });
}

function normalizeSummaryLine(line) {
  return line.replace(/^\s*[-*]\s*/, "").trim();
}

function extractBlock(body, startPatterns) {
  const lines = body.split(/\r?\n/);
  const results = [];
  let capture = false;
  for (const rawLine of lines) {
    const line = rawLine.trim();
    if (!capture && startPatterns.some((pattern) => pattern.test(line))) {
      capture = true;
    }
    if (capture) {
      if (line) {
        results.push(normalizeSummaryLine(rawLine));
      }
    }
  }
  return results;
}

function summarizeResults(sections, output) {
  const moduleMap = [
    { key: "Lexer", label: "Lexer", title: "LEXICAL ANALYZER", resultPattern: null, errorPatterns: [/^ERROR\s+/i, /^LEXICAL ERROR/i] },
    { key: "RDP", label: "Recursive Descent Parser", title: "RECURSIVE DESCENT PARSER", resultPattern: /RDP RESULT:\s*(ACCEPTED|REJECTED)/i, errorPatterns: [/^\[Syntax\]/i, /^ERROR\s+/i] },
    { key: "Predictive", label: "Predictive Parser", title: "NON-RECURSIVE PREDICTIVE PARSER", resultPattern: /PREDICTIVE RESULT:\s*(ACCEPTED|REJECTED)/i, errorPatterns: [/^\[Syntax\]/i, /^ERROR\s+/i, /no LL\(1\) table entry/i] },
    { key: "Semantic", label: "Symbol Table / Semantic Pass", title: "SEMANTIC ANALYZER", resultPattern: null, errorPatterns: [/^\[Semantic\]/i, /^ERROR\s+/i] },
    { key: "ErrorHandler", label: "Error Handler", title: null, resultPattern: null, errorPatterns: [/^\[Syntax\]/i, /^\[Semantic\]/i, /^ERROR\s+/i, /Error summary:/i] },
    { key: "LR", label: "LR Parser", title: "LR PARSER", resultPattern: /LR PARSER RESULT:\s*(ACCEPTED|REJECTED)/i, errorPatterns: [/^\[Syntax\]/i, /^ERROR\s+/i] },
  ];

  const cards = moduleMap.map((module) => {
    const section = module.title ? sections.find((entry) => entry.title.toUpperCase().includes(module.title)) : null;
    let status = "UNKNOWN";
    let reasons = [];

    if (section) {
      const resultMatch = module.resultPattern ? section.body.match(module.resultPattern) : null;
      if (resultMatch) {
        status = resultMatch[1].toUpperCase();
      } else if (module.key === "Semantic") {
        status = /ERROR\s+/i.test(section.body) ? "REJECTED" : "ACCEPTED";
      } else if (module.key === "Lexer") {
        status = /ERROR\s+/i.test(section.body) || /LEXICAL ERROR/i.test(section.body) ? "REJECTED" : "ACCEPTED";
      }
      reasons = extractBlock(section.body, module.errorPatterns);
      if (!reasons.length && status === "ACCEPTED") {
        reasons = ["No errors reported."];
      }
      if (!reasons.length && status === "REJECTED") {
        reasons = ["The compiler reported a rejection but did not emit a detailed diagnostic block."];
      }
    } else if (module.key === "ErrorHandler") {
      const errorLines = output.split(/\r?\n/).filter((line) => /^(\[Syntax\]|\[Semantic\]|ERROR\s+|Error summary:)/i.test(line.trim()));
      if (errorLines.length) {
        status = "REJECTED";
        reasons = errorLines;
      } else {
        status = "ACCEPTED";
        reasons = ["No errors reported."];
      }
    } else {
      status = "NOT RUN";
      reasons = ["This module did not appear in the current output."];
    }

    return { label: module.label, status, reasons };
  });

  const acceptedCount = cards.filter((card) => card.status === "ACCEPTED").length;
  const rejectedCount = cards.filter((card) => card.status === "REJECTED").length;
  const overallStatus = rejectedCount > 0 ? "REJECTED" : "ACCEPTED";

  return { overallStatus, acceptedCount, rejectedCount, cards };
}

function renderFinalResults(sections, output) {
  const summary = summarizeResults(sections, output);
  finalView.innerHTML = `
    <div class="final-summary final-${summary.overallStatus.toLowerCase()}">
      <div class="final-hero">
        <div>
          <div class="final-label">Overall Result</div>
          <div class="final-value">${summary.overallStatus}</div>
        </div>
        <div class="final-counts">
          <div><span>Accepted</span><strong>${summary.acceptedCount}</strong></div>
          <div><span>Rejected</span><strong>${summary.rejectedCount}</strong></div>
        </div>
      </div>
      <div class="final-cards">
        ${summary.cards.map((card) => `
          <article class="final-card final-${card.status.toLowerCase().replace(/[^a-z]+/g, "-")}">
            <div class="final-card-head">
              <h3>${escapeHtml(card.label)}</h3>
              <span>${escapeHtml(card.status)}</span>
            </div>
            <pre>${escapeHtml(card.reasons.join("\n"))}</pre>
          </article>
        `).join("")}
      </div>
    </div>
  `;
}

function renderVisualAST(rawText) {
    if (!rawText || !rawText.includes("ABSTRACT SYNTAX TREE DUMP")) {
        return '<div class="alert alert-warning m-3">No valid AST dump markers found in compiler trace.</div>';
    }

    const parts = rawText.split("=== ABSTRACT SYNTAX TREE DUMP ===");
    if (parts.length < 2) return '<div class="alert alert-danger m-3">AST Boundary breakdown failed.</div>';
    
    const treeContent = parts[1].split("=================================")[0];
    const lines = treeContent.split('\n').filter(line => line.trim().startsWith('-'));

    if (lines.length === 0) return '<div class="alert alert-info m-3">AST Tree block is empty.</div>';

    let html = '<ul class="ast-tree-view">';
    let currentLevel = 0;

    lines.forEach(line => {
        const match = line.match(/^(\s*)-\s*\[(.*?)\]\s*(.*)$/);
        if (!match) return;

        const spaces = match[1].length;
        const level = Math.floor(spaces / 2);
        const kind = match[2].trim();
        const value = match[3].trim();

        let badgeClass = 'badge-assignment';
        if (kind.toLowerCase().includes('program')) badgeClass = 'badge-program';
        else if (kind.toLowerCase().includes('decl')) badgeClass = 'badge-decl';
        else if (kind.toLowerCase().includes('expr')) badgeClass = 'badge-binary';
        else if (kind.toLowerCase().includes('id') || kind.toLowerCase().includes('identifier')) badgeClass = 'badge-identifier';
        else if (kind.toLowerCase().includes('num') || kind.toLowerCase().includes('number')) badgeClass = 'badge-number';
        else if (kind.toLowerCase().includes('while') || kind.toLowerCase().includes('loop')) badgeClass = 'badge-while';

        const nodeCard = `
            <div class="ast-node-card">
                <span class="trace-badge ${badgeClass} me-2">${kind}</span>
                <span class="node-value fw-bold" style="color: var(--text);">${value ? escapeHtml(value) : '⚙️'}</span>
            </div>
        `;

        if (level > currentLevel) {
            html += '<ul>';
        } else if (level < currentLevel) {
            for (let i = 0; i < (currentLevel - level); i++) {
                html += '</li></ul>';
            }
            html += '</li>';
        } else if (html !== '<ul class="ast-tree-view">') {
            html += '</li>';
        }

        html += `<li>${nodeCard}`;
        currentLevel = level;
    });

    for (let i = 0; i < currentLevel; i++) {
        html += '</li></ul>';
    }
    html += '</li></ul>';
    
    return html;
}

function activateTab(name) {
  tabs.forEach((tab) => tab.classList.toggle("active", tab.dataset.tab === name));
  document.querySelectorAll(".tabview").forEach((view) => {
    view.classList.toggle("active", view.id === `${name}View`);
  });
}

function activateDocTab(name) {
  docTabs.forEach((tab) => tab.classList.toggle("active", tab.dataset.docTab === name));
  docViews.forEach((view) => {
    view.classList.toggle("active", view.id === `${name}View`);
  });
}

async function fetchJSON(url, options = {}) {
  const response = await fetch(url, options);
  const data = await response.json();
  if (!response.ok) {
    throw new Error(data.error || `Request failed: ${response.status}`);
  }
  return data;
}

async function loadSamples() {
  const data = await fetchJSON("/api/samples");
  sampleSelect.innerHTML = data.samples.map((sample) => `<option value="${sample}">${sample}</option>`).join("");
}

async function loadDocs() {
  const docs = await fetchJSON("/api/docs");
  if (integrationText) integrationText.textContent = docs.integration;

  const grammarTable = parseGrammarTable(docs.grammar || "");
  grammarText.innerHTML = grammarTable ? renderTable(grammarTable.headers, grammarTable.rows) : `<pre class="trace-pre">${escapeHtml(docs.grammar || "")}</pre>`;
  const firstFollowTable = parseFirstFollowTable(docs.first_follow || "");
  const ll1Table = parseLl1Table(docs.ll1 || "");
  const lrDocTable = parseDocLrTable(docs.lr || "");
  firstFollowText.innerHTML = firstFollowTable ? renderTable(firstFollowTable.headers, firstFollowTable.rows) : `<pre class="trace-pre">${escapeHtml(docs.first_follow || "")}</pre>`;
  ll1Text.innerHTML = ll1Table ? renderTable(ll1Table.headers, ll1Table.rows) : `<pre class="trace-pre">${escapeHtml(docs.ll1 || "")}</pre>`;
  lrText.innerHTML = lrDocTable ? renderTable(lrDocTable.headers, lrDocTable.rows) : `<pre class="trace-pre">${escapeHtml(docs.lr || "")}</pre>`;
}

async function loadSampleSource(name) {
  const data = await fetchJSON(`/api/sample?name=${encodeURIComponent(name)}`);
  currentSampleSource = data.source;
  editor.value = data.source;
  samplePreview.value = data.source;
}

async function runPayload(payload) {
  setStatus("Starting Compiler Pipeline...", "running", false);
  
  const statuses = [
    "Lexical Analysis in progress...",
    "Building Abstract Syntax Tree...",
    "Running Recursive Descent Parser...",
    "Running Predictive LL(1) Parser...",
    "Running Canonical LR(1) Parser...",
    "Checking Semantics & Symbol Table...",
    "Finalizing Output..."
  ];
  
  let step = 0;
  const statusInterval = setInterval(() => {
    if (step < statuses.length) {
      setStatus(statuses[step], "running", true);
      step++;
    }
  }, 150);

  traceView.innerHTML = "";
  rawView.textContent = "";
  finalView.innerHTML = "";
  try {
    const data = await fetchJSON("/api/run", {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify(payload),
    });
    
    clearInterval(statusInterval);
    
    rawView.textContent = data.output || "";
    const astHTML = renderVisualAST(data.output || "");
    const astContainer = document.getElementById('ast-visual-container');
    if (astContainer) astContainer.innerHTML = astHTML;

    const sections = splitTrace(data.output || "");
    renderTrace(sections);
    renderFinalResults(sections, data.output || "");
    setStatus(data.returncode === 0 ? "Compiler Pipeline Finished Successfully!" : `Compiler Exited with Diagnostics (Code ${data.returncode})`, data.returncode === 0 ? "ok" : "fail", true);
    activateTab("trace");
  } catch (error) {
    clearInterval(statusInterval);
    setStatus(error.message, "fail", true);
  }
}

loadSampleButton.addEventListener("click", async () => {
  await loadSampleSource(sampleSelect.value);
});

sampleSelect.addEventListener("change", async () => {
  await loadSampleSource(sampleSelect.value);
});

runSampleButton.addEventListener("click", async () => {
  await runPayload({ mode: "sample", sample: sampleSelect.value });
});

runCustomButton.addEventListener("click", async () => {
  await runPayload({ mode: "custom", source: editor.value });
});

resetEditorButton.addEventListener("click", async () => {
  if (currentSampleSource) {
    editor.value = currentSampleSource;
  }
});

tabs.forEach((tab) => {
  tab.addEventListener("click", () => activateTab(tab.dataset.tab));
});

docTabs.forEach((tab) => {
  tab.addEventListener("click", () => activateDocTab(tab.dataset.docTab));
});

(async () => {
  try {
    await loadSamples();
    await loadDocs();
    if (sampleSelect.options.length) {
      await loadSampleSource(sampleSelect.value);
    }
    activateTab("trace");
    activateDocTab("grammar");
    setStatus("Ready", "idle");
  } catch (error) {
    setStatus(error.message, "fail");
  }
})();
