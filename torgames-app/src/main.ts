import { createApp } from 'vue'
import { createPinia } from 'pinia'
import App from './App.vue'
import router from './router'

// Vuetify
import 'vuetify/styles'
import { createVuetify } from 'vuetify'
import * as components from 'vuetify/components'
import * as directives from 'vuetify/directives'
import '@mdi/font/css/materialdesignicons.css'
import './styles/main.css'

const vuetify = createVuetify({
  components,
  directives,
  theme: {
    defaultTheme: 'dark',
    themes: {
      dark: {
        dark: true,
        colors: {
          primary: '#38bdf8', // Ocean Blue
          secondary: '#2dd4bf', // Teal/Aqua
          background: '#0f172a', // Deep Slate
          surface: '#1e293b', // Slate 800
          'surface-variant': '#334155', // Slate 700
          error: '#f43f5e', // Rose
          info: '#0ea5e9', // Sky Blue
          success: '#10b981', // Emerald
          warning: '#f59e0b', // Amber
        },
      },
      light: {
        dark: false,
        colors: {
          primary: '#2563eb', // Royal Blue
          secondary: '#475569', // Slate Grey
          background: '#f8fafc', // Slate 50
          surface: '#ffffff', // White
          'surface-variant': '#f1f5f9', // Slate 100
          error: '#dc2626', // Red
          info: '#0ea5e9', // Sky Blue
          success: '#16a34a', // Green
          warning: '#d97706', // Amber
        },
      },
    },
  },
})

const pinia = createPinia()

createApp(App)
  .use(pinia)
  .use(router)
  .use(vuetify)
  .mount('#app')
