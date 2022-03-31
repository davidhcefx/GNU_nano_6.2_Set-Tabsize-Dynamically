# GNU nano (6.2) - Set Tabsize Dynamically

[![build](https://github.com/davidhcefx/GNU-nano-6.2_Set-Tabsize-Dynamically/actions/workflows/build.yml/badge.svg)](https://github.com/davidhcefx/GNU-nano-6.2_Set-Tabsize-Dynamically/actions/workflows/build.yml)

Some files use two spaces for indentation (eg. [HTML](https://www.w3schools.com/html/html5_syntax.asp)), while others use four (eg. [Python](https://peps.python.org/pep-0008/#indentation)). In the middle of editing a file, I always wish I could change the tab width without reopening the file (which would result in loss of history). This patch adds that functionality!


### New rcfile option:
```
settabsize
    Prompts for a new tabsize to be set.
```
Example usage:
```nanorc
bind M-4 settabsize
```

## How to compile

*Please refer to [README.GIT](/README.GIT) for more details.*

1. Install the dependencies: `apt install autoconf automake autopoint gcc gettext git groff make pkg-co>

2. Run `./autogen.sh`.

3. After that, just do the normal `./configure`, `make` and `make install`.


## Modification Summary
```diff
doc/nanorc.5
@@ -719,0 +720,3 @@ Justifies the entire current buffer (or the marked region).
+.B settabsize
+Prompts for a new tabsize to be set.
+.TP

src/global.c
@@ -661,0 +662 @@ void shortcut_init(void)
+   const char *settabsize_gist = N_("Set new tabsize");
@@ -1077,0 +1079,3 @@ void shortcut_init(void)
+
+   add_to_funcs(do_set_tabsize, MMAIN,
+           N_("Set Tabsize"), WITHORSANS(settabsize_gist), TOGETHER, NOVIEW);

src/prototypes.h
@@ -478,0 +479 @@ void do_mark(void);
+void do_set_tabsize(void);

src/rcfile.c
@@ -412,0 +413,2 @@ keystruct *strtosc(const char *input)
+   else if (!strcmp(input, "settabsize"))
+       s->func = do_set_tabsize;

src/text.c
@@ -60,0 +61,22 @@ void do_mark(void)
+
+/* Prompt user to set the new tabsize. We use the spell menu because
+ * it has no functions. */
+void do_set_tabsize(void)
+{
+   ssize_t new_tabsize = -1;
+   int response = do_prompt(MSPELL, "", NULL, edit_refresh, "New tabsize");
+
+   /* Cancel if no answer provided. */
+   if (response != 0) {
+       statusbar(_("Cancelled"));
+       return;
+   }
+
+   if (!parse_num(answer, &new_tabsize) || new_tabsize <= 0) {
+       statusline(AHEM, _("Requested tab size \"%s\" is invalid"), answer);
+       return;
+   }
+
+   tabsize = new_tabsize;
+   statusline(REMARK, _("Tabsize set to %d"), tabsize);
+}
```
