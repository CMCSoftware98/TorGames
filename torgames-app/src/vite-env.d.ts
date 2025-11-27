/// <reference types="vite/client" />

// Declare vuetify styles module (CSS imports)
declare module 'vuetify/styles' {
  const styles: string
  export default styles
}

// Declare vuetify components and directives
declare module 'vuetify/components' {
  import type { Component } from 'vue'
  const components: Record<string, Component>
  export = components
}

declare module 'vuetify/directives' {
  import type { Directive } from 'vue'
  const directives: Record<string, Directive>
  export = directives
}
