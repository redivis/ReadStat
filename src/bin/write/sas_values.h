/* Redivis: formats SAS numeric values for CSV output the way Redivis's previous SAS converter (sas-convert, built on
 * the Parso library) did. See sas_values.c */
int sas_value_string(double value, const char *format, char *buf, size_t len);
