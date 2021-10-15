#!/usr/bin/python3
import png
import sys

x_color = (150, 150, 150)
dot_color = (50, 50, 50)
false_x_color = (255, 0, 0)
false_dot_color = (255, 255, 0)


def process_input(input_file_name):
    img = []
    with open(input_file_name, "r") as input_file:
        content = input_file.read()
        content = content.splitlines()
        input_height = len(content)
        input_width = None
        for line in content:
            if input_width is None:
                input_width = len(line)
            else:
                assert input_width == len(line)
            row = ()
            for char in line:
                if char == 'x':
                    row += x_color
                elif char == '.':
                    row += dot_color
            img.append(row)
    return img, input_width, input_height


def compare_inputs(input_file_name1, input_file_name2):
    image = []
    with open(input_file_name1) as input_file1, open(input_file_name2) as input_file2:
        content1 = input_file1.read().splitlines()
        content2 = input_file2.read().splitlines()
        assert len(content1) == len(content2)
        input_width = None
        input_height = len(content1)
        for line1, line2 in zip(content1, content2):
            assert len(line1) == len(line2)
            if input_width is None:
                input_width = len(line1)
            else:
                assert input_width == len(line1)
            row = ()
            for char1, char2 in zip(line1, line2):
                if char1 == char2:
                    if char1 == 'x':
                        row += x_color
                    elif char2 == '.':
                        row += dot_color
                    else:
                        assert False
                else:
                    if char1 == 'x' and char2 == '.':
                        row += false_x_color
                    elif char1 == '.' and char2 == 'x':
                        row += false_dot_color
                    else:
                        assert False
            image.append(row)
    return image, input_width, input_height


def create_picture(img, output_file_name):
    with open(output_file_name, 'wb') as f:
        w = png.Writer(width, height, greyscale=False)
        w.write(f, img)


if __name__ == "__main__":
    if len(sys.argv) != 4:
        print(f"Usage: {sys.argv[0]} input_file1 input_file2 output_file")
        print("Compares input_file1 to input_file2 and creates a difference image. input_file2 is "
              "treated as the 'right' input data. (only usable for the mandelbrot sets)")
        exit(1)
    # img, width, height = process_input(sys.argv[1])
    img, width, height = compare_inputs(sys.argv[1], sys.argv[2])
    assert (width is not None and height is not None)
    create_picture(img, sys.argv[3])
