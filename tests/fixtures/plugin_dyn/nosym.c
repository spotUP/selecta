/* Fixture module without sel_plugin_entry (I427a reject case): a library the loader opens but cannot use. */
int selfix_not_an_entry(void) {
    return 7;
}
