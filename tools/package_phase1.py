"""Package the historical Phase 1 Windows build without launching it."""
import sys
sys.dont_write_bytecode = True
from package_game import main

if __name__ == "__main__":
    main(3, "Phase1")
