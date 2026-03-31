import { defineConfig } from 'vite'
import react from '@vitejs/plugin-react'

// https://vite.dev/config/
export default defineConfig({
  plugins: [react()],
  server: {
    host: '0.0.0.0', // Ép Vite lắng nghe trên tất cả các card mạng
    port: 5173,      // Cổng mặc định
    strictPort: true // Đảm bảo không bị nhảy sang port khác nếu 5173 bận
  }
})
