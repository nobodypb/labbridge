// UI navigation helpers exercise the same compact global menus on both sizes.
export async function openHeaderAction(page,id){
 const dialog=await page.locator('#'+id).getAttribute('data-open-dialog');
 if(dialog&&await page.locator('#'+dialog).isVisible())return;
 if(!await page.locator('#'+id).isVisible())await page.locator('#moreMenu').click();
 await page.locator('#'+id).click();
}
export async function openAccount(page){if(await page.locator('#accountMenu').count()){await page.locator('#benchAuth button').filter({hasText:'Sign out'}).waitFor({state:'attached'});if(!await page.locator('#accountDialog').isVisible())await openHeaderAction(page,'accountMenu');}}
export async function openDevicePanel(page){if(!await page.locator('#devicePanel').isVisible())await page.locator('#devicePanelToggle').click();}
