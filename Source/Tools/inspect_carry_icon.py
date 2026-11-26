import unreal
for path in ['/AST_InventorySystem/UI/ItemIconComponent','/AST_InventorySystem/UI/InteractionWidget','/AST_InventorySystem/UI/ItemIcon']:
 a=unreal.load_asset(path)
 unreal.log('ICON_INSPECT '+path+' '+str(a))
 if a and hasattr(a,'generated_class'):
  c=unreal.get_default_object(a.generated_class())
  for n in ['widget_class','space','draw_size','relative_location']:
   try: unreal.log('ICON_PROPERTY '+n+' '+str(c.get_editor_property(n)))
   except: pass
