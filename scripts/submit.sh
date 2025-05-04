#!/bin/bash
git add .

# get users input for commit message
echo "Enter commit message: "
read commit_message

# check if commit message is empty
# if empty, set to default message
if [ -z "$commit_message" ]; then
    commit_message="update"
fi

# commit changes
git commit -m "$commit_message"

# push changes to remote repository
git push origin master
