#include <bits/stdc++.h>

using namespace std;


pair<int,int> find_group_of_k(string &s, int start, int k)
{
    int first = -1;
    int last = -1;
    int count = 0;

    for(int i = start;i < s.length();i++)
    {
        if(s[i] == 'a')
        {
            if(first == -1)
                first = i;
            last = i;
            if(++count == k)
                break;
        }
    }
    return {first, last};
}

int solution(string s) {
    int n = s.length();
    int num_of_a = std::count(s.begin(), s.end(), 'a');
    if(s.length() < 3)
        return 0;
    if(num_of_a == 0)
        return (n-2) * (n-1) / 2;
    if(num_of_a % 3 != 0)
        return 0;
    int sub_num_of_a = num_of_a / 3;

    auto first_group = find_group_of_k(s, 0, sub_num_of_a);
    auto second_group = find_group_of_k(s, first_group.second+1, sub_num_of_a);
    auto third_group = find_group_of_k(s, second_group.second+1, sub_num_of_a);
    return (second_group.first - first_group.second) * 
    (third_group.first - second_group.second);
}


vector<int> solution2(vector<int> A, vector<int> B) {
    vector<int> ans;
    int carry = 0;
    int n =A.size(), m = B.size();
    for(int i = 0; i < max(n, m);i++)
    {
        int sum = 
                (i < n ? A[i] : 0) +
                (i < m ? B[i] : 0) +
                carry;
        ans.push_back(abs(sum) % 2);
        if(sum == 0 || sum == 1)
            carry = 0;
        else if(sum > 0)
            carry = -1;
        else
            carry = 1;
    }

    if(carry == 1)
        ans.push_back(1);
    else if(carry == -1)
    {
        ans.push_back(1);
        ans.push_back(1);
    }

    while(ans.back() == 0 && ans.size() > 0)
        ans.pop_back();
    return ans;
}

int main()
{

    cout << solution("abbabababababb");
    return 0;
}