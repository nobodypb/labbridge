import {defineConfig} from '@playwright/test';
const port=Number(process.env.LABBRIDGE_WEB_PORT??4173);
export default defineConfig({
 testDir:'./browser',workers:1,outputDir:'../.tools/browser-results',
 use:{baseURL:`http://127.0.0.1:${port}`,browserName:'chromium',headless:true,
  launchOptions:process.env.LABBRIDGE_CHROMIUM_EXECUTABLE?{executablePath:process.env.LABBRIDGE_CHROMIUM_EXECUTABLE}:{}},
 webServer:{command:'node tests/serve.js',url:`http://127.0.0.1:${port}`,reuseExistingServer:!process.env.CI},
});
