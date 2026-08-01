// @ts-check
import { defineConfig } from "astro/config";
import vue from "@astrojs/vue";
import tailwindcss from "@tailwindcss/vite";

// retrace website — Astro 7 + Vite 8 + Tailwind 4 + Vue islands
// Deployed via .github/workflows/website.yml to GitHub Pages.
export default defineConfig({
  site: "https://riboseinc.github.io",
  // GitHub Pages serves the repo at /retrace/, so the build output
  // must use that base. Local `astro dev` is unaffected.
  base: "/retrace/",
  output: "static",
  integrations: [vue()],
  vite: {
    plugins: [tailwindcss()],
  },
});
