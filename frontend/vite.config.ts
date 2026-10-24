import { fileURLToPath, URL } from 'node:url';
import { defineConfig, loadEnv } from 'vite';
import react from '@vitejs/plugin-react';

const contracts = fileURLToPath(new URL('../contracts/examples', import.meta.url));

export default defineConfig(({ mode }) => {
  const env = loadEnv(mode, process.cwd(), 'VITE_');
  const target = env.VITE_API_BASE || 'http://localhost:8080';

  return {
    plugins: [react()],
    resolve: {
      alias: { '@contracts': contracts },
    },
    server: {
      port: 5173,
      strictPort: true,
      // Mock fixtures live outside frontend/.
      fs: { allow: ['.', contracts] },
      proxy: {
        '/api': target,
        '/oauth': target,
        '/audio': target,
        '/health': target,
        '/ws': { target, ws: true, changeOrigin: true },
      },
    },
  };
});
