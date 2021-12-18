import  os
import sys
import re


class style:
    class_name = ""
    style_text = ""
    def __init__(self, name, text):
        self.class_name = name
        self.style_text = text
        pass
    def __str__(self) -> str:
        return f"Style of {self.class_name}: {self.style_text}"
    
    def __repr__(self) -> str:
        return f'style="{self.style_text}"'

    # def format(self):
        # pass


if __name__ ==  "__main__":
    if len(sys.argv) < 2:
        print("Require argument: File, Use `python3 style-parser.py <file> to convert`")
    file = sys.argv[1]
    stylelist = []
    svgstring = ""
    # Read Styles
    with open(file=file, encoding='utf8',mode='r') as f:
        lines = f.readlines()
        if len(lines) > 1:
            print("Unsupported")
        else:
<<<<<<< HEAD
            svgstring = lines[0]
            styletext = re.search(r"<style>(.*?)</style>",lines[0],).group()
            styletext = styletext[7:-8]
            stylenode = re.findall(r"\..*?\}",styletext)
=======
            # print(lines)
            svgstring = lines[0]
            styletext = re.search(r"<style>(.*?)</style>",lines[0],).group()
            _styletext = styletext[7:-8]
            stylenode = re.findall(r"\..*?\}",_styletext)
>>>>>>> 7b2071bb7bb45c03dfb93de1a39232a71569e4d2
            # print(styletext)
            # print(stylenode)
            
            for node in stylenode:
                this_style_name = re.search(r"\..*?{",node).group()[1:-1]
                this_style_text = re.search(r"{.*?}",node).group()[1:-1]
                stylelist.append(style(this_style_name,this_style_text))
            
            for style in stylelist:
<<<<<<< HEAD
                print(f"replacing class={style.class_name} to {style.__repr__()}")
                svgstring.replace(f'class={style.class_name}',f'{style.__repr__()}')
=======
                origin = str(f'class="{style.class_name}"')
                alter = str(style.__repr__())
                print(f'replacing {origin} to {alter}')
                # s = svgstring.find(origin)
                # print(s)
                # svgstring.replace(origin,alter)
                svgstring = svgstring.replace(origin,alter)
                # re.search(rf"{origin}")
                # print(svgstring)
            svgstring = svgstring.replace(styletext,'')
>>>>>>> 7b2071bb7bb45c03dfb93de1a39232a71569e4d2
            print (svgstring)
            
    # print(stylelist)
    with open(file=file, encoding='utf8',mode='w+') as f:
<<<<<<< HEAD
        
            pass
=======
        f.write(svgstring)
    #         pass
>>>>>>> 7b2071bb7bb45c03dfb93de1a39232a71569e4d2
    # print(file)