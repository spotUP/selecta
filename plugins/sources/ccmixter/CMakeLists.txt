# ccMixter streaming source plugin (row I725g). Uses only the host net and catalogue services.
sel_add_plugin(ccmixter 7 ccmixter.c)
target_link_libraries(sel_plugin_ccmixter PUBLIC selecta_shared_source)
