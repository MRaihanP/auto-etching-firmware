/** @type {import('tailwindcss').Config} */
export default {
  content: ['./index.html', './src/**/*.{ts,tsx}'],
  theme: {
    extend: {
      colors: {
        ink: '#0b1220',
        panel: '#111b2e',
        line: '#22314c',
        safe: '#35d07f',
        warn: '#f5b942',
        danger: '#ff5d5d',
      },
    },
  },
  plugins: [],
};
