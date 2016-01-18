#kernel for mint2g

This repository cotains the kernel source of a kernel which has not been named cause there is no unique features in it *yet?* for mint2g (Samsung Galaxy Star/Star Duos).

The base for the kernel is stock-uni as it has some updates than other stock kernels. See the respective commit in the stock-uni-nodelete of the stock kernel repo for the full changeset. In short theres update to wifi driver, battery gauge and some configs.

##Building:
You can just clone the 'master' branch for the stable version for building. It's build ready anytime (or at least it should). 
`git clone -b master https://github.com/phalf/kernel_mint2g.git` If you want to contribute see the section below. 

##Contributing
Pull requests are welcome. If you want to contribute, branch off from 'testing' with the name that is a short description of your added features/fix, commit your changes, and if testing updates before your pull request is submited, rebase then create a pull request.

You can also create pull request with unfinished features. Just suffix the branch name with '-wip'.

This is a my slightly modified github flow. Please see https://guides.github.com/introduction/flow/ for the core concept.