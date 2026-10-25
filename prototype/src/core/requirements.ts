/** Extract only an explicit dress-code line; do not infer what the user is wearing. */
export function preparationRequirements(text: string): string[] {
  const line = text.split(/\r?\n/).find(line => /^\s*dress\s*code\s*:/i.test(line));
  if (!line || /\b(not|optional|no requirement|not required)\b/i.test(line)) return [];
  return /\b(business professional|business formal|full suit)\b/i.test(line)
    ? ['Business professional: full suit'] : [];
}
