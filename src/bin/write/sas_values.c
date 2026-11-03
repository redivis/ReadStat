/* Redivis: formats SAS numeric values for CSV output the way Redivis's previous SAS converter (sas-convert, built on
 * the Parso library) did, so that SAS uploads keep converting to the same values:
 *
 * - Values with a date or datetime format (the format names Parso 2.0.14 recognizes) are written as yyyy-MM-dd if
 *   they fall on midnight, else yyyy-MM-dd HH:mm:ss, truncated to the second. SAS counts dates in days, and datetimes
 *   in seconds, from 1960-01-01. Time formats (TIME, HHMM, ...) stay numbers of seconds, as in Parso.
 * - Other values within 1e-14 of an integer are written as that integer, and the rest in the shortest decimal (with
 *   no exponent) that reads back as the same double, as Java's DecimalFormat does.
 * - Positive values below 1e-300 are treated as missing, as in Parso.
 */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "sas_values.h"

static const char *sas_date_formats[] = {
    "DATE", "DAY", "DDMMYY", "DDMMYYB", "DDMMYYC", "DDMMYYD", "DDMMYYN", "DDMMYYP", "DDMMYYS",
    "MMDDYY", "MMDDYYB", "MMDDYYC", "MMDDYYD", "MMDDYYN", "MMDDYYP", "MMDDYYS",
    "YYMMDD", "YYMMDDB", "YYMMDDC", "YYMMDDD", "YYMMDDN", "YYMMDDP", "YYMMDDS",
    "MMYY", "MMYYC", "MMYYD", "MMYYN", "MMYYP", "MMYYS", "YYMM", "YYMMC", "YYMMD", "YYMMN", "YYMMP", "YYMMS",
    "JULIAN", "JULDAY", "MONTH", "YEAR", "MONYY", "YYMON", "B8601DA", "E8601DA", "MONNAME",
    "WEEKDATE", "WEEKDATX", "WEEKDAY", "DOWNAME", "WORDDATE", "WORDDATX", "QTR",
    NULL
};

static const char *sas_datetime_formats[] = {
    "DATETIME", "B8601DN", "B8601DT", "B8601DX", "B8601DZ", "B8601LX", "E8601DN", "E8601DT", "E8601DX", "E8601DZ",
    "E8601LX", "DATEAMPM", "DTDATE", "DTMONYY", "DTWKDATX", "DTYEAR", "MDYAMPM", "TOD",
    NULL
};

#define SECONDS_PER_DAY 86400

static int is_in_list(const char *name, const char **list) {
    for (int i=0; list[i]; i++) {
        if (strcmp(name, list[i]) == 0)
            return 1;
    }
    return 0;
}

/* A format's name, without its width and digits: e.g. MMDDYY for MMDDYY10. and E8601DT for E8601DT19.3 */
static void sas_format_name(const char *format, char *name, size_t len) {
    size_t name_len = strcspn(format, ".");
    while (name_len > 0 && format[name_len-1] >= '0' && format[name_len-1] <= '9')
        name_len--;
    if (name_len >= len)
        name_len = len - 1;
    memcpy(name, format, name_len);
    name[name_len] = '\0';
}

/* Days since 1970-01-01 to a proleptic Gregorian date (Howard Hinnant's civil_from_days) */
static void civil_from_days(long long days, long long *year, int *month, int *day) {
    days += 719468;
    long long era = (days >= 0 ? days : days - 146096) / 146097;
    long long doe = days - era * 146097;
    long long yoe = (doe - doe/1460 + doe/36524 - doe/146096) / 365;
    long long doy = doe - (365*yoe + yoe/4 - yoe/100);
    long long mp = (5*doy + 2)/153;
    *day = (int)(doy - (153*mp + 2)/5 + 1);
    *month = (int)(mp < 10 ? mp + 3 : mp - 9);
    *year = yoe + era * 400 + (*month <= 2);
}

/* Seconds since 1960-01-01 as a date, or a datetime unless it's midnight. Returns 0 if out of range */
static int sas_seconds_string(double seconds, char *buf, size_t len) {
    /* Beyond years 1-9999, which BigQuery can't store anyway */
    if (!(fabs(seconds) < 3.2e11))
        return 0;
    long long whole_seconds = (long long)floor(seconds);
    long long days = whole_seconds / SECONDS_PER_DAY;
    long long time_of_day = whole_seconds % SECONDS_PER_DAY;
    if (time_of_day < 0) {
        time_of_day += SECONDS_PER_DAY;
        days--;
    }
    long long year;
    int month, day;
    /* 1960-01-01 is 3653 days before 1970-01-01 */
    civil_from_days(days - 3653, &year, &month, &day);
    if (year < 1 || year > 9999)
        return 0;
    if (time_of_day == 0) {
        snprintf(buf, len, "%04lld-%02d-%02d", year, month, day);
    } else {
        snprintf(buf, len, "%04lld-%02d-%02d %02lld:%02lld:%02lld", year, month, day,
                time_of_day / 3600, (time_of_day / 60) % 60, time_of_day % 60);
    }
    return 1;
}

/* The shortest decimal that reads back as the same double, without an exponent. Any double whose shortest form has
 * 15 significant digits or fewer reads back from its 15-digit rounding (with trailing zeros, which are stripped below),
 * so only 15, 16 and 17 digits need trying */
static void shortest_decimal_string(double value, char *buf, size_t len) {
    char sci[40];
    int precision;
    for (precision=15; precision<=17; precision++) {
        snprintf(sci, sizeof(sci), "%.*e", precision-1, value);
        if (strtod(sci, NULL) == value)
            break;
    }

    /* Split e.g. -1.2345e-07 into its sign, digits and exponent */
    const char *p = sci;
    int negative = (*p == '-');
    if (negative)
        p++;
    char digits[20];
    int digit_count = 0;
    while (*p && *p != 'e') {
        if (*p >= '0' && *p <= '9' && digit_count < (int)sizeof(digits)-1)
            digits[digit_count++] = *p;
        p++;
    }
    digits[digit_count] = '\0';
    while (digit_count > 1 && digits[digit_count-1] == '0')
        digits[--digit_count] = '\0';
    int exponent = (*p == 'e') ? atoi(p+1) : 0;

    /* The decimal point goes after digit number (exponent + 1) */
    int point = exponent + 1;
    size_t pos = 0;
    if (negative && pos < len-1)
        buf[pos++] = '-';
    if (point <= 0) {
        if (pos < len-1)
            buf[pos++] = '0';
        if (pos < len-1)
            buf[pos++] = '.';
        for (int i=0; i<-point && pos < len-1; i++)
            buf[pos++] = '0';
        for (int i=0; i<digit_count && pos < len-1; i++)
            buf[pos++] = digits[i];
    } else {
        for (int i=0; i<point && pos < len-1; i++)
            buf[pos++] = i < digit_count ? digits[i] : '0';
        if (digit_count > point) {
            if (pos < len-1)
                buf[pos++] = '.';
            for (int i=point; i<digit_count && pos < len-1; i++)
                buf[pos++] = digits[i];
        }
    }
    buf[pos] = '\0';
}

/* Writes value to buf. Returns 0 if it should be written as missing */
int sas_value_string(double value, const char *format, char *buf, size_t len) {
    if (isnan(value) || (value > 0 && value < 1e-300))
        return 0;
    if (isinf(value)) {
        snprintf(buf, len, value > 0 ? "Infinity" : "-Infinity");
        return 1;
    }

    if (format && format[0]) {
        char name[64];
        sas_format_name(format, name, sizeof(name));
        if (is_in_list(name, sas_datetime_formats) && sas_seconds_string(value, buf, len))
            return 1;
        if (is_in_list(name, sas_date_formats) && fabs(value) < 3.7e6
                && sas_seconds_string(value * SECONDS_PER_DAY, buf, len))
            return 1;
    }

    double rounded = round(value);
    if (fabs(value - rounded) < 1e-14 && fabs(rounded) < 9.2e18) {
        snprintf(buf, len, "%lld", (long long)rounded);
        return 1;
    }
    shortest_decimal_string(value, buf, len);
    return 1;
}
