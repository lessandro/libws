all:
	$(MAKE) -C ws
	$(MAKE) -C example

clean:
	$(MAKE) -C ws clean
	$(MAKE) -C example clean
