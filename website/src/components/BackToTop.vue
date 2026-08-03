<script setup>
import { ref, onMounted, onBeforeUnmount } from "vue";

const visible = ref(false);
let onScroll = null;

onMounted(() => {
  onScroll = () => {
    visible.value = window.scrollY > 800;
  };
  onScroll();
  window.addEventListener("scroll", onScroll, { passive: true });
});

onBeforeUnmount(() => {
  if (onScroll) window.removeEventListener("scroll", onScroll);
});

function scrollTop() {
  window.scrollTo({
    top: 0,
    behavior: window.matchMedia &&
      window.matchMedia("(prefers-reduced-motion: reduce)").matches
      ? "auto"
      : "smooth",
  });
}
</script>

<template>
  <Transition name="fade">
    <button
      v-if="visible"
      class="back-to-top"
      @click="scrollTop"
      aria-label="Back to top"
      title="Back to top"
    >
      <svg viewBox="0 0 16 16" width="14" height="14" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round">
        <path d="M8 13V3M3 8l5-5 5 5" />
      </svg>
    </button>
  </Transition>
</template>

<style scoped>
.back-to-top {
  position: fixed;
  bottom: 28px;
  right: 28px;
  width: 44px;
  height: 44px;
  border-radius: 50%;
  background: linear-gradient(180deg, rgba(28, 33, 42, 0.85) 0%, rgba(20, 24, 30, 0.75) 100%);
  backdrop-filter: blur(22px) saturate(180%);
  -webkit-backdrop-filter: blur(22px) saturate(180%);
  border: 1px solid rgba(255, 255, 255, 0.1);
  color: var(--color-text);
  cursor: pointer;
  display: flex;
  align-items: center;
  justify-content: center;
  box-shadow:
    inset 0 1px 0 rgba(255, 255, 255, 0.1),
    0 12px 32px -8px rgba(0, 0, 0, 0.5);
  z-index: 60;
  transition: transform 0.2s var(--ease-glass), border-color 0.2s, color 0.2s;
}
.back-to-top:hover {
  transform: translateY(-2px);
  border-color: rgba(94, 227, 255, 0.4);
  color: var(--color-see);
}
.back-to-top:active {
  transform: translateY(0);
}
.back-to-top svg {
  display: block;
}

@media (max-width: 540px) {
  .back-to-top {
    bottom: 18px;
    right: 18px;
    width: 40px;
    height: 40px;
  }
}

.fade-enter-active, .fade-leave-active {
  transition: opacity 0.25s, transform 0.25s;
}
.fade-enter-from, .fade-leave-to {
  opacity: 0;
  transform: translateY(8px);
}

@media (prefers-reduced-motion: reduce) {
  .fade-enter-active, .fade-leave-active {
    transition: opacity 0.01ms;
  }
}
</style>
