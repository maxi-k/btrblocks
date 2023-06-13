import sys
import yaml

valid_types = ["integer", "smallint", "double", "float", "string"]

def fix_format(lines, columns):
    for i in range(len(lines)):
        line = lines[i]
        line = line.strip()
        fields = line.split("|")
        assert(len(fields) == len(columns))

        for idx in range(len(fields)):
            if columns[idx]["type"] == "double":
                if fields[idx] == "null":
                    pass
                elif fields[idx] == "":
                    fields[idx] = "null"
                else:
                    f = float(fields[idx])
                    fields[idx] = f"{f:.6f}"
            elif columns[idx]["type"] == "integer":
                if fields[idx] == "":
                    fields[idx] = "null"

        lines[i] = "|".join(fields)


# Remove extra fields from original lines
def strip_original_lines(original_lines, columns):
    global valid_types
    for i in range(len(original_lines)):
        line = original_lines[i]
        parts = line.split("|")
        assert(len(parts) == len(columns))

        stripped_line = ""
        first = True
        for part, column in zip(parts, columns):
            if column["type"] not in valid_types:
                continue
            if first:
                stripped_line = part
                first = False
            else:
                stripped_line = f"{stripped_line}|{part}"

        original_lines[i] = stripped_line


def main():
    global valid_types
    if len(sys.argv) != 4:
        print(f"Usage: {sys.argv[0]} <btrcsv> <originalcsv> <schema.yaml>", file=sys.stderr)
        sys.exit(1)

    print("Reading in Btr CSV")
    with open(sys.argv[1]) as btrcsv:
        btr_lines = btrcsv.readlines()

    print("Reading in Original CSV")
    with open(sys.argv[2]) as originalcsv:
        original_lines = originalcsv.readlines()

    if (len(original_lines) != len(btr_lines)):
        print(f"Numbers of lines do not match. Btr has {len(btr_lines)}. Original has {len(original_lines)}.")
        sys.exit(1)
    print(f"CSVs have {len(original_lines)} lines")

    with open(sys.argv[3]) as schema:
        columns = yaml.safe_load(schema)["columns"]

    print("Stripping original lines")
    # Strip original lines of extra fields
    strip_original_lines(original_lines, columns)

    # The columns we ignore do not exist in the csv anymore after converting back from btr
    btr_columns = [c for c in columns if c["type"] in valid_types]

    # Format lines
    print("Formatting btr lines")
    fix_format(btr_lines, btr_columns)
    print("Formatting original lines")
    fix_format(original_lines, btr_columns)

    print("Sorting btr lines")
    btr_lines.sort()
    print("Sorting original lines")
    original_lines.sort()

    print("Diffing lines")
    assert(len(btr_lines) == len(original_lines))
    total_diff = 0
    for k, o in zip(btr_lines, original_lines):
        if k == o:
            continue

        if total_diff == 0:
            print("The following lines differ")
            print(k)
            print(o)
        total_diff += 1

    if total_diff > 0:
        print(f"In total {total_diff} lines differ")
        sys.exit(1)

    print("CSVs do not differ")

if __name__ == "__main__":
    main()
