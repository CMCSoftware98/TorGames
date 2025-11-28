<script setup lang="ts">
// App.vue is now just a router-view shell
</script>

<template>
  <router-view />
</template>

<style>
/* Global overrides that need to be in App.vue */
html, body {
  overflow: hidden; /* Prevent scroll on body */
}
</style>
