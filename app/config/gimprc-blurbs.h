/*  gimprc-blurbs.h  --  descriptions for gimprc properties  */

#ifndef __GIMP_RC_BLURBS_H__
#define __GIMP_RC_BLURBS_H__


/*  Not all strings defined here are used in the user interface
 *  (the preferences dialog mainly) and only those that are should
 *  be marked for translation.
 */

#define BRUSH_PATH_BLURB \
"Sets the brush search path."

#define CANVAS_PADDING_MODE_BLURB \
N_("Specifies how the area around the image should be drawn.")

#define CANVAS_PADDING_COLOR_BLURB \
N_("Sets the canvas padding color used if the padding mode is set to " \
   "custom color.")

#define FS_CANVAS_PADDING_MODE_BLURB \
N_("Specifies how the area around the image should be drawn when in " \
   "fullscreen mode.")

#define FS_CANVAS_PADDING_COLOR_BLURB \
N_("Sets the canvas padding color used when in fullscreen mode and " \
   "the padding mode is set to custom color.")

#define COLORMAP_CYCLING_BLURB \
N_("Specify that marching ants for selected regions will be drawn with " \
   "colormap cycling as opposed to be drawn as animated lines.  This color " \
   "cycling option works only with 8-bit displays.")

#define CONFIRM_ON_CLOSE_BLURB \
N_("Ask for confirmation before closing an image without saving.")

#define CURSOR_MODE_BLURB \
N_("Sets the mode of cursor the GIMP will use.")

#define CURSOR_UPDATING_BLURB \
N_("Context-dependent cursors are cool.  They are enabled by default. " \
   "However, they require overhead that you may want to do without.")

#define DEFAULT_BRUSH_BLURB \
"Specify a default brush.  The brush is searched for in the " \
"specified brush path."

#define DEFAULT_COMMENT_BLURB \
"Sets the default comment."

#define DEFAULT_DOT_FOR_DOT_BLURB \
N_("When enabled, this will ensure that each pixel of an image gets " \
   "mapped to a pixel on the screen.")

#define DEFAULT_FONT_BLURB \
"Specify a default font.  The font is searched for in the " \
"fontconfig font path."

#define DEFAULT_GRADIENT_BLURB \
"Specify a default gradient.  The gradient is searched for in the " \
"specified gradient path."

#define DEFAULT_IMAGE_WIDTH_BLURB \
"Sets the default image width in the \"File/New\" dialog."

#define DEFAULT_IMAGE_HEIGHT_BLURB \
"Sets the default image height in the \"File/New\" dialog."

#define DEFAULT_IMAGE_TYPE_BLURB \
"Sets the default image type in the \"File/New\" dialog."

#define DEFAULT_PATTERN_BLURB \
"Specify a default pattern. The pattern is searched for in the " \
"specified pattern path."

#define DEFAULT_PALETTE_BLURB \
"Specify a default palette.  The palette is searched for in the " \
"specified palette path."

#define DEFAULT_RESOLUTION_UNIT_BLURB \
"Sets the units for the display of the default resolution in the " \
"\"File/New\" dialog."

#define DEFAULT_THRESHOLD_BLURB \
N_("Tools such as fuzzy-select and bucket fill find regions based on a " \
   "seed-fill algorithm.  The seed fill starts at the intially selected " \
   "pixel and progresses in all directions until the difference of pixel " \
   "intensity from the original is greater than a specified threshold. " \
   "This value represents the default threshold.")

#define DEFAULT_UNIT_BLURB \
"Sets the default unit for new images and for the \"File/New\" dialog. " \
"This units will be used for coordinate display when not in dot-for-dot " \
"mode."

#define DEFAULT_XRESOLUTION_BLURB \
"Sets the default horizontal resolution for new images and for the " \
"\"File/New\" dialog. This value is always in dpi (dots per inch)."

#define DEFAULT_YRESOLUTION_BLURB \
"Sets the default vertical resolution for new images and for the " \
"\"File/New\" dialog. This value is always in dpi (dots per inch)."

#define ENVIRON_PATH_BLURB \
"Sets the environ search path."

#define FRACTALEXPLORER_PATH_BLURB \
"Where to search for fractals used by the Fractal Explorer plug-in."

#define GAMMA_CORRECTION_BLURB \
"This setting is ignored."
#if 0
"Sets the gamma correction value for the display. 1.0 corresponds to no " \
"gamma correction.  For most displays, gamma correction should be set " \
"to between 2.0 and 2.6. One important thing to keep in mind: Many images " \
"that you might get from outside sources will in all likelihood already " \
"be gamma-corrected.  In these cases, the image will look washed-out if " \
"the GIMP has gamma-correction turned on.  If you are going to work with " \
"images of this sort, turn gamma correction off by setting the value to 1.0."
#endif

#define GFIG_PATH_BLURB \
"Where to search for Gfig figures used by the Gfig plug-in."

#define GFLARE_PATH_BLURB \
"Where to search for gflares used by the GFlare plug-in."

#define GIMPRESSIONIST_PATH_BLURB \
"Where to search for data used by the Gimpressionist plug-in."

#define GRADIENT_PATH_BLURB \
"Sets the gradient search path."

#define HELP_BROWSER_BLURB \
N_("Sets the browser used by the help system.")

#define IMAGE_STATUS_FORMAT_BLURB \
N_("Sets the text to appear in image window status bars.")

#define IMAGE_TITLE_FORMAT_BLURB \
N_("Sets the text to appear in image window titles.")

#define INFO_WINDOW_PER_DISPLAY_BLURB \
N_("When enabled, the GIMP will use a different info window per image view.")

#define INITIAL_ZOOM_TO_FIT_BLURB \
N_("When enabled, this will ensure that the full image is visible after a " \
   "file is opened, otherwise it will be displayed with a scale of 1:1.")

#define INSTALL_COLORMAP_BLURB \
N_("Install a private colormap; might be useful on pseudocolor visuals.")

#define INTERPOLATION_TYPE_BLURB \
N_("Sets the level of interpolation used for scaling and other " \
   "transformations.")

#define LAST_OPENED_SIZE_BLURB \
N_("How many recently opened image filenames to keep on the File menu.")

#define MARCHING_ANTS_SPEED_BLURB \
N_("Speed of marching ants in the selection outline.  This value is in " \
   "milliseconds (less time indicates faster marching).")

#define MAX_NEW_IMAGE_SIZE_BLURB  \
N_("GIMP will warn the user if an attempt is made to create an image that " \
   "would take more memory than the size specified here.")

#define MIN_COLORS_BLURB  \
N_("Generally only a concern for 8-bit displays, this sets the minimum " \
   "number of system colors allocated for the GIMP.")

#define MODULE_PATH_BLURB \
"Sets the module search path."

#define MODULE_LOAD_INHIBIT_BLURB \
"To inhibit loading of a module, add its name here."

#define MONITOR_RES_FROM_GDK_BLURB \
"When enabled, the GIMP will use the monitor resolution from the " \
"windowing system."

#define MONITOR_XRESOLUTION_BLURB \
"Sets the monitor's horizontal resolution, in dots per inch.  If set to " \
"0, forces the X server to be queried for both horizontal and vertical " \
"resolution information."

#define MONITOR_YRESOLUTION_BLURB \
"Sets the monitor's vertical resolution, in dots per inch.  If set to " \
"0, forces the X server to be queried for both horizontal and vertical " \
"resolution information."

#define NAVIGATION_PREVIEW_SIZE_BLURB \
N_("Sets the size of the navigation preview available in the lower right " \
   "corner of the image window.")

#define NUM_PROCESSORS_BLURB \
N_("On multiprocessor machines, if GIMP has been compiled with --enable-mp " \
   "this sets how many processors GIMP should use simultaneously.")

#define PALETTE_PATH_BLURB \
"Sets the palette search path."

#define PATTERN_PATH_BLURB \
"Sets the pattern search path."

#define PERFECT_MOUSE_BLURB \
N_("When enabled, the X server is queried for the mouse's current position " \
   "on each motion event, rather than relying on the position hint.  This " \
   "means painting with large brushes should be more accurate, but it may " \
   "be slower.  Perversely, on some X servers enabling this option results " \
   "in faster painting.")

#define PLUG_IN_PATH_BLURB \
"Sets the plug-in search path."

#define PLUGINRC_PATH_BLURB \
"Sets the pluginrc search path."

#define LAYER_PREVIEWS_BLURB \
N_("Sets whether GIMP should create previews of layers and channels. " \
   "Previews in the layers and channels dialog are nice to have but they " \
   "can slow things down when working with large images.")

#define LAYER_PREVIEW_SIZE_BLURB \
N_("Sets the default preview size for layers and channels.")

#define RESIZE_WINDOWS_ON_RESIZE_BLURB \
N_("When enabled, the image window will automatically resize itself, " \
   "whenever the physical image size changes.")

#define RESIZE_WINDOWS_ON_ZOOM_BLURB \
N_("When enabled, the image window will automatically resize itself, " \
   "when zooming into and out of images.")

#define RESTORE_SESSION_BLURB \
N_("Let GIMP try to restore your last saved session on each startup.")

#define SAVE_DEVICE_STATUS_BLURB \
N_("Remember the current tool, pattern, color, and brush across GIMP " \
   "sessions.")

#define SAVE_SESSION_INFO_BLURB \
N_("Save the positions and sizes of the main dialogs when the GIMP exits.")

#define SCRIPT_FU_PATH_BLURB \
"This path will be searched for scripts when the Script-Fu plug-in is run."

#define SHOW_BRUSH_OUTLINE_BLURB \
N_("When enabled, all paint tools will show a preview of the current " \
   "brush's outline.")

#define SHOW_MENUBAR_BLURB \
N_("When enabled, the menubar is visible by default. This can also be " \
   "toggled with the \"View->Show Menubar\" command.")

#define SHOW_RULERS_BLURB \
N_("When enabled, the rulers are visible by default. This can also be " \
   "toggled with the \"View->Show Rulers\" command.")

#define SHOW_SCROLLBARS_BLURB \
N_("When enabled, the scrollbars are visible by default. This can also be " \
   "toggled with the \"View->Show Scrollbars\" command.")

#define SHOW_STATUSBAR_BLURB \
N_("When enabled, the statusbar is visible by default. This can also be " \
   "toggled with the \"View->Show Statusbar\" command.")

#define FS_SHOW_MENUBAR_BLURB \
N_("When enabled, the menubar is visible by default in fullscreen mode. "\
   "This can also be toggled with the \"View->Show Menubar\" command.")

#define FS_SHOW_RULERS_BLURB \
N_("When enabled, the rulers are visible by default in fullscreen mode. "\
   "This can also be toggled with the \"View->Show Rulers\" command.")

#define FS_SHOW_SCROLLBARS_BLURB \
N_("When enabled, the scrollbars are visible by default in fullscreen mode. "\
   "This can also be toggled with the \"View->Show Scrollbars\" command.")

#define FS_SHOW_STATUSBAR_BLURB \
N_("When enabled, the statusbar is visible by default in fullscreen mode. "\
   "This can also be toggled with the \"View->Show Statusbar\" command.")

#define SHOW_TIPS_BLURB \
N_("Enable to display a handy GIMP tip on startup.")

#define SHOW_TOOL_TIPS_BLURB \
N_("Enable to display tooltips.")

#define STINGY_MEMORY_USE_BLURB \
N_("There is always a tradeoff between memory usage and speed.  In most " \
   "cases, the GIMP opts for speed over memory.  However, if memory is a " \
   "big issue, try to enable this setting.")

#define SWAP_PATH_BLURB \
N_("Sets the swap file location. The gimp uses a tile based memory " \
   "allocation scheme. The swap file is used to quickly and easily " \
   "swap tiles out to disk and back in. Be aware that the swap file " \
   "can easily get very large if the GIMP is used with large images. " \
   "Also, things can get horribly slow if the swap file is created on " \
   "a directory that is mounted over NFS.  For these reasons, it may " \
   "be desirable to put your swap file in \"/tmp\".")

#define TEAROFF_MENUS_BLURB \
N_("When enabled, menus can be torn off.")

#define CAN_CHANGE_ACCELS_BLURB \
N_("When enabled, you can change keyboard shortcuts for menu items on the fly.")

#define SAVE_ACCELS_BLURB \
N_("Save changed keyboard shortcuts when the GIMP exits.")

#define RESTORE_ACCELS_BLURB \
N_("Restore saved keyboard shortcuts on each GIMP startup.")

#define TEMP_PATH_BLURB \
N_("Sets the temporary storage directory. Files will appear here " \
   "during the course of running the GIMP.  Most files will disappear " \
   "when the GIMP exits, but some files are likely to remain, so it is " \
   "best if this directory not be one that is shared by other users.")

#define THEME_BLURB \
"The name of the theme to use."

#define THEME_PATH_BLURB \
"Sets the theme search path."

#define THUMBNAIL_SIZE_BLURB \
N_("Sets the size of the thumbnail saved with each image. Note that GIMP " \
   "can not save thumbnails if layer previews are disabled.")

#define TILE_CACHE_SIZE_BLURB \
N_("The tile cache is used to make sure the GIMP doesn't thrash " \
   "tiles between memory and disk. Setting this value higher will " \
   "cause the GIMP to use less swap space, but will also cause " \
   "the GIMP to use more memory. Conversely, a smaller cache size " \
   "causes the GIMP to use more swap space and less memory.")

#define TRANSPARENCY_TYPE_BLURB \
N_("Sets the manner in which transparency is displayed in images.")

#define TRANSPARENCY_SIZE_BLURB \
N_("Sets the size of the checkerboard used to display transparency.")

#define TRUST_DIRTY_FLAG_BLURB \
N_("When enabled, the GIMP will not save if the image is unchanged since " \
   "opening it.")

#define UNDO_LEVELS_BLURB \
N_("Sets the minimal number of operations that can be undone. More undo " \
   "levels are kept available until the undo-size limit is reached.")

#define UNDO_SIZE_BLURB \
N_("Sets an upper limit to the memory that is used per image to keep " \
   "operations on the undo stack.")

#define USE_HELP_BLURB  \
N_("When enabled, pressing F1 will open the help browser.")


#endif  /* __GIMP_RC_BLURBS_H__ */
