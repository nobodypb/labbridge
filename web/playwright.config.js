import {defineConfig} from '@playwright/test';
export default defineConfig({
 testDir:'./browser',workers:1,outputDir:'../.tools/browser-results',
 use:{baseURL:'http://127.0.0.1:4173',browserName:'chromium',headless:true,
  launchOptions:process.env.LABBRIDGE_CHROMIUM_EXECUTABLE?{executablePath:process.env.LABBRIDGE_CHROMIUM_EXECUTABLE}:{}},
 webServer:{command:'node tests/serve.js',url:'http://127.0.0.1:4173',reuseExistingServer:!process.env.CI},
});
