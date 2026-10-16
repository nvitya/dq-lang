const blockClosers = {
  if: "endif",
  for: "endfor",
  while: "endwhile",
  func: "endfunc",
  subfn: "endsubfn",
  object: "endobject",
  struct: "endstruct"
};

function blockCloserForHeader(line) {
  const match = /^(\s*)(if|for|while|func|subfn|object|struct)\b.*:\s*$/.exec(line);
  if (!match) return undefined;

  return {
    indentation: match[1],
    text: blockClosers[match[2]]
  };
}

function hasIndentedBodyLine(lines, indentation) {
  return lines.some(line => {
    const lineIndentation = /^\s*/.exec(line)[0];
    return line.trim() &&
      lineIndentation.startsWith(indentation) &&
      lineIndentation.length > indentation.length;
  });
}

module.exports = { blockCloserForHeader, hasIndentedBodyLine };
