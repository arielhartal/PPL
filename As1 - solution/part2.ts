import * as R from "ramda";

const stringToArray = R.split("");

/* Question 1 */
export const countLetters: (s: string) => {} = R.pipe(
    R.toLower,
    stringToArray,
    R.filter((c) => c != " "),
    R.sortBy(R.identity),
    (chars: string[]) => R.groupWith(R.equals, chars),
    R.reduce((acc : {} , curr : string[]) => R.assoc(curr[0], R.length(curr), acc), {})
);

/* Question 2 */
const popIfMatch = (stack: string[], openParen: string, closeParen: string): string[] =>
    R.isEmpty(stack) ? [closeParen] :
        R.head(stack) === openParen ? R.tail(stack) : R.prepend(closeParen, stack);

const isRightParentheses = (c: string): boolean =>
    c === "(" || c === "{" || c === "[";

const isPairedReducer = (stack: string[], c: string): string[] =>
    isRightParentheses(c) ? R.prepend(c, stack) :
        c === ")" ? popIfMatch(stack, "(", c) :
        c === "}" ? popIfMatch(stack, "{", c) :
        c === "]" ? popIfMatch(stack, "[", c) :
        stack;

export const isPaired: (s: string) => boolean = R.pipe(
    stringToArray,
    R.reduce(isPairedReducer, []),
    R.isEmpty
);

/* Question 3 */
export type WordTree = {
    root: string;
    children: WordTree[];
}

export const treeToSentence = (t: WordTree): string =>
    t.children.length === 0 ? t.root :
        t.root + " " + R.map(treeToSentence,t.children).join(" ");


