install:
	@echo "--- 🚀 Installing project dependencies ---"
	pip install -e ".[dev]"

lint:
	@echo "--- 🧹 Running linters ---"
	ruff format . 			# running ruff formatting
	ruff check . --fix  	# running ruff linting
