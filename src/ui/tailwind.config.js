/** @type {import('tailwindcss').Config} */
module.exports = {
  content: ['./src/**/*.{html,ts}', './src/app/**/*.{html,ts}'],
  theme: {
    extend: {
      colors: {
        brand: {
          DEFAULT: '#1f6feb',
          foreground: '#ffffff',
        },
      },
    },
  },
  plugins: [require('daisyui')],
  daisyui: {
    themes: [
      {
        corporate: {
          primary: '#1f6feb',
          secondary: '#0ea5e9',
          accent: '#22c55e',
          neutral: '#1f2937',
          'base-100': '#ffffff',
        },
      },
    ],
  },
};
