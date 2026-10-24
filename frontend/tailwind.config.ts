import type { Config } from 'tailwindcss';

// Plain neutral palette. T21 owns the visual identity and will replace this.
export default {
  content: ['./index.html', './src/**/*.{ts,tsx}'],
  theme: { extend: {} },
  plugins: [],
} satisfies Config;
