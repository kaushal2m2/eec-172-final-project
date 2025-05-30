When testing use

> npm i && npm run dev

to deploy, run

> npm run build npm run deploy

Also, push code to the repo from the react-migration branch, using

> git add .
> git commit -m "message"
> git push

If it says something about set upstream, copy that command and run it

In package.json, use
 
> "homepage": "http://localhost:5173",

to test, and

> "homepage": "https://kaushal2m2.github.io/eec-172-final-project/",

when deploying.