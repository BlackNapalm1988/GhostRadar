from test_sensors import test_data
from english_words import english_words_set
from colorama import Fore, Style

def setup():
    
    "Setup the ESP32 and intialize the wifi, lcd and menu"
    
    def intialize_wifi():
        print("Setting up WiFi...")
    
    def initialize_lcd():
        print("Setting up LCD...")
    
    def intialize_menu():
        print("Setting up Menu...")
    
    while True:

        intialize_wifi()
        initialize_lcd()
        intialize_menu()
        
        print("Connecting to WiFi...")
        print("Connecting to LCD...")
        print("Building Menu...", "\n")
    
        return False
    
def main():
    
    letters = ['a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n', 'o', 'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y', 'z']
    buffer = ""
    
    # Change this value to adjust the length of the buffer to search inside for a word.
    buffer_max_length = 25
    
    # 3 appears to be the best, however, it does produce quite a bit of words. This may be resolved if more sensors are added to boost complexity.
    word_min_length = 3 
    
    # Run this once only during the intial boot and during reset
    setup() 
    
    # Generates a letter map from the incoming data to match the length of the available letters.
    letter_map = sorted([letters[x % len(letters)] for x in range(0, 300)]) 
    
    # Checks to see if the word has reached the word length. This can be adjusted, however, it will only look for words that are shorter than the word length.
    while len(buffer) < buffer_max_length:
        buffer += letter_map[test_data() - 1]
        
        for word in english_words_set:
            
            if len(word) > word_min_length:
                
                if word in buffer:
                    print(f"{Fore.GREEN}{word}{Style.RESET_ALL}", end="")
                    buffer = ""
                    
                elif len(buffer) == buffer_max_length:
                    buffer = ""
                    continue
        else:
            print(f"{Fore.RED}\u002E{Style.RESET_ALL}",  end="")


        
if __name__ == "__main__":
    main()