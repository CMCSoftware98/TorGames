<script setup lang="ts">
// App.vue is now just a router-view shell
</script>

<template>
  <router-view />
</template>

<style>
html, body {
  overflow: hidden !important;
  height: 100vh !important;
  max-height: 100vh !important;
  margin: 0 !important;
  padding: 0 !important;
}

.v-application {
  height: 100vh !important;
  max-height: 100vh !important;
  overflow: hidden !important;
}

.v-application__wrap {
  min-height: 100vh !important;
  max-height: 100vh !important;
  height: 100vh !important;
  display: flex !important;
  flex-direction: column !important;
}

/* Remove Vuetify's default layout spacing */
.v-application {
  --v-layout-bottom: 0px !important;
}

.v-window {
  flex: 1 1 0 !important;
  min-height: 0 !important;
  overflow: hidden !important;
}

.v-window__container {
  height: 100% !important;
}

.v-window-item {
  height: 100% !important;
  overflow: hidden !important;
}

.v-footer {
  flex-shrink: 0 !important;
  margin: 0 !important;
}
</style>
